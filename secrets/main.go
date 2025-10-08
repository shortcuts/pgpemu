package main

import (
	"bufio"
	"bytes"
	"encoding/base64"
	"encoding/hex"
	"flag"
	"fmt"
	"hash/crc32"
	"log"
	"os"
	"strconv"
	"strings"
	"time"

	"gopkg.in/yaml.v2"

	"github.com/tarm/serial"
)

type DeviceSecret struct {
	Name string `yaml:"name"`
	Mac  string `yaml:"mac"`
	Key  string `yaml:"key"`
	Blob string `yaml:"blob"`
}

type Secrets struct {
	Devices []DeviceSecret `yaml:"devices"`
}

// Helper to decode hex or base64
func decodeField(s string, expectedLen int, fieldName string) ([]byte, error) {
	var res []byte
	var err error

	// Try hex first
	res, err = hex.DecodeString(s)
	if err == nil && len(res) == expectedLen {
		return res, nil
	}
	// Try base64
	res, err = base64.StdEncoding.DecodeString(s)
	if err == nil && len(res) == expectedLen {
		return res, nil
	}
	if err == nil {
		err = fmt.Errorf("decoded %s is wrong length: got %d, want %d", fieldName, len(res), expectedLen)
	}
	return nil, fmt.Errorf("couldn't decode %s: %v", fieldName, err)
}

func printLine(line []byte) {
	fmt.Printf("> %s\n", strings.TrimSpace(string(line)))
}

func serialCommand(ser *serial.Port, command string, value any) bool {
	if !strings.Contains("NMKB", command) {
		log.Printf("invalid command: %s", command)
		return false
	}

	_, err := ser.Write([]byte(command))
	if err != nil {
		log.Printf("serial write error: %v", err)
		return false
	}
	line, err := bufio.NewReader(ser).ReadBytes('\n')
	printLine(line)
	if !bytes.Contains(line, []byte("set="+command)) {
		log.Printf("expected set=%s, got: %s", command, line)
		return false
	}

	var sendData []byte
	switch v := value.(type) {
	case string:
		if command == "N" {
			if len(v) > 15 {
				log.Printf("name too long: %s", v)
				return false
			}
			sendData = []byte(v)
			_, err := ser.Write(sendData)
			if err != nil {
				log.Printf("serial write error: %v", err)
				return false
			}
			ser.Write([]byte("\n"))
		}
	case []byte:
		wantedLens := map[string]int{"M": 6, "K": 16, "B": 256}
		if len(v) != wantedLens[command] {
			log.Printf("invalid len for %s: %d", command, len(v))
			return false
		}
		sendData = []byte(base64.StdEncoding.EncodeToString(v))
		_, err := ser.Write(sendData)
		if err != nil {
			log.Printf("serial write error: %v", err)
			return false
		}
		ser.Write([]byte("\n"))
	default:
		log.Printf("invalid arguments to function")
		return false
	}

	line, err = bufio.NewReader(ser).ReadBytes('\n')
	printLine(line)
	if !bytes.Contains(line, []byte(command+"=[OK]")) {
		log.Printf("command failed: %s, value: %v", command, value)
		log.Printf("sent(%d): %s", len(sendData), string(sendData))
		return false
	}
	return true
}

func main() {
	var (
		serialPort  = flag.String("port", "", "Serial port (e.g. /dev/ttyUSB0)")
		secretsFile = flag.String("file", "", "Secrets YAML file")
		baud        = flag.Int("baud", 115200, "Baud rate")
		timeout     = flag.Int("timeout", 2, "Read timeout (seconds)")
	)
	flag.Parse()
	if *serialPort == "" || *secretsFile == "" {
		fmt.Printf("Usage: %s -file secrets.yaml -port /dev/ttyUSBx [-baud 115200] [-timeout 2]\n", os.Args[0])
		os.Exit(1)
	}

	fmt.Println("Reading input secrets file...")
	data, err := os.ReadFile(*secretsFile)
	if err != nil {
		log.Fatalf("Failed to read file: %v", err)
	}

	var secrets Secrets
	if err := yaml.Unmarshal(data, &secrets); err != nil {
		log.Fatalf("Failed to parse yaml: %v", err)
	}
	if len(secrets.Devices) == 0 {
		log.Println("No secrets present")
		return
	}

	c := &serial.Config{Name: *serialPort, Baud: *baud, ReadTimeout: time.Second * time.Duration(*timeout)}
	ser, err := serial.OpenPort(c)
	if err != nil {
		log.Fatalf("Failed to open serial port: %v", err)
	}
	defer ser.Close()

	fmt.Println("Entering secrets mode...")
	ser.Write([]byte("X"))
	scanner := bufio.NewScanner(ser)
	for scanner.Scan() {
		line := scanner.Bytes()
		printLine(line)
		if bytes.Equal(line, []byte("!")) {
			break
		}
	}

	fmt.Println("Listing current secrets:")
	ser.Write([]byte("l"))
	for scanner.Scan() {
		line := scanner.Bytes()
		fmt.Println(strings.TrimSpace(string(line)))
		if len(line) == 0 {
			break // End of list
		}
	}

	fmt.Println("Uploading secrets...")
	for i, secret := range secrets.Devices {
		mac, err := decodeField(secret.Mac, 6, "mac")
		if err != nil {
			log.Printf("Skipping device %d: %v", i, err)
			continue
		}
		key, err := decodeField(secret.Key, 16, "key")
		if err != nil {
			log.Printf("Skipping device %d: %v", i, err)
			continue
		}
		blob, err := decodeField(secret.Blob, 256, "blob")
		if err != nil {
			log.Printf("Skipping device %d: %v", i, err)
			continue
		}

		ser.Write([]byte(strconv.Itoa(i)))
		scanner.Scan()
		line := scanner.Bytes()
		printLine(line)
		if !bytes.Contains(line, []byte(fmt.Sprintf("slot=%d", i))) {
			log.Printf("Failed selecting slot: %s", string(line))
			continue
		}
		if !serialCommand(ser, "N", secret.Name) {
			log.Printf("Failed to upload name for slot %d", i)
			continue
		}
		if !serialCommand(ser, "M", mac) {
			log.Printf("Failed to upload MAC for slot %d", i)
			continue
		}
		if !serialCommand(ser, "K", key) {
			log.Printf("Failed to upload key for slot %d", i)
			continue
		}
		if !serialCommand(ser, "B", blob) {
			log.Printf("Failed to upload blob for slot %d", i)
			continue
		}
		// get CRC
		ser.Write([]byte("s"))
		scanner.Scan()
		line = scanner.Bytes()
		printLine(line)
		wantedCrcInt := crc32.ChecksumIEEE(append(append(mac, key...), blob...))
		wantedCrcStr := fmt.Sprintf("%08x", wantedCrcInt)
		if !bytes.Contains(line, []byte("slot=tmp crc="+wantedCrcStr)) {
			log.Printf("Wrong CRC in tmp buf! Wanted: %s", wantedCrcStr)
			continue
		}
		// write to nvs
		ser.Write([]byte("W"))
		scanner.Scan()
		line = scanner.Bytes()
		printLine(line)
		if !bytes.Contains(line, []byte("[OK]")) {
			log.Printf("Writing failed for slot %d", i)
			continue
		}
		scanner.Scan()
		line = scanner.Bytes()
		printLine(line)
		if !bytes.Contains(line, []byte("write=1")) {
			log.Printf("Writing result bad for slot %d", i)
			continue
		}
		fmt.Printf("Writing to NVS slot %d OK\n", i)
		// read back stored data's CRC
		ser.Write([]byte("S"))
		scanner.Scan()
		line = scanner.Bytes()
		printLine(line)
		if !bytes.Contains(line, []byte(fmt.Sprintf("slot=%d crc=%s", i, wantedCrcStr))) {
			log.Printf("Wrong CRC in NVS! Wanted: %s", wantedCrcStr)
			continue
		}
		fmt.Println("Readback OK")
	}

	fmt.Println("\nListing new secrets:")
	ser.Write([]byte("l"))
	for scanner.Scan() {
		line := scanner.Bytes()
		fmt.Println(strings.TrimSpace(string(line)))
		if len(line) == 0 {
			break
		}
	}
	fmt.Println("Leaving secrets mode...")
	ser.Write([]byte("q"))
	for scanner.Scan() {
		line := scanner.Bytes()
		printLine(line)
		if bytes.Equal(line, []byte("X")) {
			break
		}
	}
	fmt.Println("OK!")
}

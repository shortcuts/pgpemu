package main

import (
	"hash/crc32"
	"testing"
)

// Test decodeField with valid hex input for MAC
func TestDecodeField_HexMac(t *testing.T) {
	input := "8c2b8a29d314"
	expected := []byte{0x8c, 0x2b, 0x8a, 0x29, 0xd3, 0x14}
	out, err := decodeField(input, 6, "mac")
	if err != nil {
		t.Fatalf("decodeField hex MAC failed: %v", err)
	}
	if !byteSliceEqual(out, expected) {
		t.Fatalf("Expected %x, got %x", expected, out)
	}
}

// Test decodeField with valid hex input for KEY
func TestDecodeField_HexKey(t *testing.T) {
	input := "2ba019f94b77d4fe20b39945a4974498"
	expected := []byte{0x2b, 0xa0, 0x19, 0xf9, 0x4b, 0x77, 0xd4, 0xfe, 0x20, 0xb3, 0x99, 0x45, 0xa4, 0x97, 0x44, 0x98}
	out, err := decodeField(input, 16, "key")
	if err != nil {
		t.Fatalf("decodeField hex KEY failed: %v", err)
	}
	if !byteSliceEqual(out, expected) {
		t.Fatalf("Expected %x, got %x", expected, out)
	}
}

func TestDecodeField_Base64Mac(t *testing.T) {
	input := "fLuKJdMk" // base64 for [0x7c, 0xbb, 0x8a, 0x25, 0xd3, 0x24]
	expected := []byte{0x7c, 0xbb, 0x8a, 0x25, 0xd3, 0x24}
	out, err := decodeField(input, 6, "mac")
	if err != nil {
		t.Fatalf("decodeField base64 MAC failed: %v", err)
	}
	if !byteSliceEqual(out, expected) {
		t.Fatalf("Expected %x, got %x", expected, out)
	}
}

// Test decodeField with wrong size
func TestDecodeField_WrongSize(t *testing.T) {
	input := "1c2b2a75d3" // only 5 bytes
	_, err := decodeField(input, 6, "mac")
	if err == nil {
		t.Fatalf("Expected error due to wrong size, got nil")
	}
}

// Test decodeField with invalid input
func TestDecodeField_InvalidInput(t *testing.T) {
	input := "not-a-hex-or-base64!"
	_, err := decodeField(input, 8, "key")
	if err == nil {
		t.Fatalf("Expected error for invalid input")
	}
}

// Helper for comparing byte slices
func byteSliceEqual(a, b []byte) bool {
	if len(a) != len(b) {
		return false
	}
	for i := range a {
		if a[i] != b[i] {
			return false
		}
	}
	return true
}

// Test CRC32 calculation for some data
func TestCRC32(t *testing.T) {
	data := []byte("nice")
	expected := crc32.ChecksumIEEE(data)
	got := crc32.ChecksumIEEE([]byte{'n', 'i', 'c', 'e'})
	if got != expected {
		t.Fatalf("Expected %08x, got %08x", expected, got)
	}
}

FILE(GLOB CSources *.c)
set(COMPONENT_SRCS ${CSources})
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()

# Create a NVS image from the contents of the `nvs_data` CSV file
# that fits the partition named 'nvs'. FLASH_IN_PROJECT indicates that
# the generated image should be flashed when the entire project is flashed to
# the target with 'idf.py -p PORT flash'.
nvs_create_partition_image(nvs ../secrets.csv FLASH_IN_PROJECT)

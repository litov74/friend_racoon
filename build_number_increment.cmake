# https://stackoverflow.com/questions/35745344/cmake-target-version-increment
# basic definitions
set(VERSION_PREFIX "Racoon.D1.")
set(VERSION_FILE "version.txt")
set(CACHE_FILE "build_number_cache.txt")

message("Build cache file is '${CACHE_FILE}'")

# Перед увеличением номера версии копируем бинарники
IF(EXISTS ${VERSION_FILE})
    file(COPY 
        build/racoon.bin
        build/ota_data_initial.bin
        build/partition_table/partition-table.bin
        build/bootloader/bootloader.bin
        ${VERSION_FILE}
        DESTINATION binaries/ )
ENDIF()

# Reading data from file + incrementation
IF(EXISTS ${CACHE_FILE})
    file(READ ${CACHE_FILE} INCREMENTED_VALUE)
    math(EXPR INCREMENTED_VALUE "${INCREMENTED_VALUE}+1")
ELSE()
    set(INCREMENTED_VALUE "1")
ENDIF()

# Update the cache
file(WRITE ${CACHE_FILE} "${INCREMENTED_VALUE}")

# Create the version
file(WRITE ${VERSION_FILE} "${VERSION_PREFIX}${INCREMENTED_VALUE}")

message("The next build number is '${VERSION_PREFIX}${INCREMENTED_VALUE}'")

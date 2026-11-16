file(GLOB_RECURSE srcs "main.c" "src/*.c" "src/ubx_gpace.c" "src/imu.c" "src/init.c" "src/ble_payload.c")

idf_component_register(SRCS "${srcs}"
                       INCLUDE_DIRS "./include")

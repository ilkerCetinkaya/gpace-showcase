#include "ble_payload.h"

int get_data(uint8_t *buffer)
{
    // Parse imu data to bytes

    union val16_to_val8 acc_x;
    union val16_to_val8 acc_y;
    union val16_to_val8 acc_z;
    union val16_to_val8 gyro_x;
    union val16_to_val8 gyro_y;
    union val16_to_val8 gyro_z;

    // Create checksum variables
    uint8_t checksum_a = 0;
    uint8_t checksum_b = 0;

    // Pvt and imu result structs are defined static to save space and cpu power

    static struct Result pvt_res = {
        .longtitude = {0},
        .latitude = {0},
        .altitude = {0},
        .altitude_msl = {0},
        .horizontal_acc = {0},
        .speed = {0},
        .heading = {0},
        .speed_acc = {0},
        .pdop = {0},
        .numSIV = 0,
        .second = 0,
        .minute = 0,
        .millisecond = {0},
    };

    static struct BMI_270_Result imu_res = {
        .acc_x = 0,
        .acc_y = 0,
        .acc_z = 0,
        .gyro_x = 0,
        .gyro_y = 0,
        .gyro_z = 0
    };

    ubx_get_pvt(&pvt_res);
    imu_readaccgyro(&imu_res);

    acc_x.val16 = imu_res.acc_x;
    acc_y.val16 = imu_res.acc_y;
    acc_z.val16 = imu_res.acc_z;
    gyro_x.val16 = imu_res.gyro_x;
    gyro_y.val16 = imu_res.gyro_x;
    gyro_z.val16 = imu_res.gyro_x;

    buffer[0] = 1; // Syncronization byte

    /* Setting the necessary data */

    buffer[1] = pvt_res.longtitude[0];
    buffer[2] = pvt_res.longtitude[1];
    buffer[3] = pvt_res.longtitude[2];
    buffer[4] = pvt_res.longtitude[3];

    buffer[5] = pvt_res.latitude[0];
    buffer[6] = pvt_res.latitude[1];
    buffer[7] = pvt_res.latitude[2];
    buffer[8] = pvt_res.latitude[3];

    buffer[9] = pvt_res.altitude[0];
    buffer[10] = pvt_res.altitude[1];
    buffer[11] = pvt_res.altitude[2];
    buffer[12] = pvt_res.altitude[3]; 

    buffer[13] = pvt_res.altitude_msl[0];
    buffer[14] = pvt_res.altitude_msl[1];
    buffer[15] = pvt_res.altitude_msl[2];
    buffer[16] = pvt_res.altitude_msl[3];

    buffer[17] = pvt_res.horizontal_acc[0];
    buffer[18] = pvt_res.horizontal_acc[1];
    buffer[19] = pvt_res.horizontal_acc[2];
    buffer[20] = pvt_res.horizontal_acc[3];

    buffer[21] = pvt_res.speed[0]; 
    buffer[22] = pvt_res.speed[1];
    buffer[23] = pvt_res.speed[2];
    buffer[24] = pvt_res.speed[3];

    buffer[25] = pvt_res.heading[0];
    buffer[26] = pvt_res.heading[1];
    buffer[27] = pvt_res.heading[2];
    buffer[28] = pvt_res.heading[3];

    buffer[29] = pvt_res.speed_acc[0];
    buffer[30] = pvt_res.speed_acc[1];
    buffer[31] = pvt_res.speed_acc[2];
    buffer[32] = pvt_res.speed_acc[3];
    
    buffer[33] = pvt_res.pdop[0];  
    buffer[34] = pvt_res.pdop[1];
  

    buffer[35] = pvt_res.numSIV;

    buffer[36] = pvt_res.second;
    
    buffer[37] = pvt_res.minute;
    
    buffer[38] = pvt_res.millisecond[0];
    buffer[39] = pvt_res.millisecond[1];
    buffer[40] = pvt_res.millisecond[2];
    buffer[41] = pvt_res.millisecond[3];

    buffer[42] = acc_x.val8[0];
    buffer[43] = acc_x.val8[1];

    buffer[44] = acc_y.val8[0];
    buffer[45] = acc_y.val8[1];

    buffer[46] = acc_z.val8[0];
    buffer[47] = acc_z.val8[1];

    buffer[48] = gyro_x.val8[0];
    buffer[49] = gyro_x.val8[1];

    buffer[50] = gyro_y.val8[0];
    buffer[51] = gyro_y.val8[1];

    buffer[52] = gyro_z.val8[0];
    buffer[53] = gyro_z.val8[1];

    buffer [54] = pvt_res.fixtype;
    
    // Calculate checksum

    checksum(&checksum_a, &checksum_b, buffer);

    buffer[55] = checksum_a;
    buffer[56] = checksum_b;

    return 1;
}

int checksum(uint8_t *checksum_a, uint8_t *checksum_b, uint8_t* buffer)
{
    *checksum_a = 0;
    *checksum_b = 0;

    *checksum_a += (55 & 0xFF);
    *checksum_b += *checksum_a;

    *checksum_a += (55 >> 8);
    *checksum_b += *checksum_a;

    for (int i = 0; i < 55; i++)
    {
        *checksum_a +=  buffer[i];
        *checksum_b += *checksum_a;       
    }
    

    return 1;
}

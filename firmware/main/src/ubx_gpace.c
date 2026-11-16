#include "ubx_gpace.h"
#include "driver/uart.h"
#include "common.h"

void calcChecksum(struct ubxMessage *msg)
{   
    // 8 bit fletcher's checksum based on the ublox documentation.

    msg->checksumA = 0;
    msg->checksumB = 0;

    msg->checksumA += msg->cls;
    msg->checksumB += msg->checksumA;

    msg->checksumA += msg->id;
    msg->checksumB += msg->checksumA;

    msg->checksumA += (msg->len & 0xFF);
    msg->checksumB += msg->checksumA;

    msg->checksumA += (msg->len >> 8);
    msg->checksumB += msg->checksumA;

    for (uint16_t i = 0; i < msg->len; i++)
    {
        msg->checksumA += msg->payload[i];
        msg->checksumB += msg->checksumA;
    }
}

int ubx_get_pvt(struct Result *buffer)
{   
    /*Automatic PVT won't be used for first version of the product for testing.
      So, we will poll pvt data every time we need. (At 25hz, it will be every 40ms) */

    struct ubxMessage msg;

    msg.cls = UBX_NAV;
    msg.id = UBX_NAV_PVT;
    msg.len = 0;
   
    uint8_t payload_data[92] = {0};
    
    int ret_write = ubx_send_message(&msg);
    int ret_read = ubx_read_message(payload_data);
    int ret_parse = ubx_parse_message(UBX_NAV_PVT, buffer, payload_data);

    uart_flush(UBX_UART_NUM);

    return (ret_write == 1)*(ret_read == 1)*(ret_parse == 1);
}


int ubx_setval(uint32_t key, uint32_t value, uint8_t layer)
{

    /* This function will set the desired cfg to desired value in desired ram and
    returns:
    1,0 : message acknowledged or not acknowledged
    -1 : error reading the message from uart (probably uart driver error)*/


    struct ubxMessage msg;
    msg.cls = UBX_CFG;
    msg.id = UBX_CFG_VALSET;
    msg.len = 4 + 4 + 4; // 4 byte header, 4 byte key ID, 2 byte value
    msg.payload = (uint8_t*) malloc(msg.len);

    // Clear packet payload
    memset(msg.payload, 0, msg.len);

    msg.payload[0] = 0;     // Message Version - set to 0
    msg.payload[1] = layer; 

    // Load key into outgoing payload
    msg.payload[4] = key >> 8 * 0; // Key LSB
    msg.payload[5] = key >> 8 * 1;
    msg.payload[6] = key >> 8 * 2;
    msg.payload[7] = key >> 8 * 3;

    // Load user's value
    msg.payload[8] = value >> 8 * 0; // Value LSB
    msg.payload[9] = value >> 8 * 1;
    msg.payload[10] = value >> 8 * 2;
    msg.payload[11] = value >> 8 * 3;

    ubx_send_message(&msg);
  
    uint8_t msg_ack[10] = {0};
   
    uart_read_bytes(UBX_UART_NUM, msg_ack, 10, 1000/portTICK_PERIOD_MS);

    /*
    for (int i = 0; i < 10; i++)
    {
 
        ESP_LOGI("UBX_SETVAL","buffer[%d]:%d",i,msg_ack[i]);
    }
    */

    uart_flush(UBX_UART_NUM);

    free(msg.payload);
    
    if ((msg_ack[0] == UBX_SYNC_1) && (msg_ack[1] == UBX_SYNC_2))
    {
        return msg_ack[3];
    }

    else
    {
        return -1;
    }
    
}


int ubx_getval(uint32_t key)
{
    struct ubxMessage msg;
    msg.cls = UBX_CFG;
    msg.id = UBX_CFG_VALGET;
    msg.len = 4 + 4 * 1; // While multiple keys are allowed, we will send only one key at a time
    msg.payload = (uint8_t*) malloc(msg.len);
  
    // Clear packet payload
    memset(msg.payload, 0, msg.len);
  
    // VALGET uses different memory layer definitions to VALSET
    // because it can only return the value for one layer.
    // So we need to fiddle the layer here.
    // And just to complicate things further, the ZED-F9P only responds
    // correctly to layer 0 (RAM) and layer 7 (Default)!
    uint8_t layer = 1;                         // 7 is the "Default Layer"

  
    msg.payload[0] = 0;        // Message Version - set to 0
    msg.payload[1] = layer; // Layer
  
    // Load key into outgoing payload
    msg.payload[4] = key >> 8 * 0; // Key LSB
    msg.payload[5] = key >> 8 * 1;
    msg.payload[6] = key >> 8 * 2;
    msg.payload[7] = key >> 8 * 3;

    ubx_send_message(&msg);

    uint8_t msg_getval[16] = {0};

    uart_read_bytes(UBX_UART_NUM, msg_getval, 16, 1000/portTICK_PERIOD_MS);

    /*

    for (int i = 0; i < 16  ; i++)
    {
       
        ESP_LOGI("UBX_GETVAL","buffer[%d]:%d",i,msg_getval[i]);

    }
    */
    
    uart_flush(UBX_UART_NUM);
    
    free(msg.payload);

    return msg_getval[14];

}


int ubx_connect(int tx, int rx)
{
    /*This function will try to communicate over uart with predefined baudrates sorted
    in most frequently used to less frequently used order.*/

    int baudrates[] = {460800, 38400, 921600, 19200, 9600, 57600, 115200, 230400};

    for (int i = 0; i < 8; i++)
    {
        const uart_config_t uart_config = {
        .baud_rate = baudrates[i],
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

        ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_install(UBX_UART_NUM, 256, 0, 0, NULL, 0));
        ESP_ERROR_CHECK_WITHOUT_ABORT(uart_param_config(UBX_UART_NUM, &uart_config));
        ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_pin(UBX_UART_NUM, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

        int ret_prot2 = ubx_setval(UBXLOX_CFG_PRT_UART1_OUTPROT_NMEA, 0, VAL_LAYER_RAM_BBR);
        if (ret_prot2 != -1)
        {
            vTaskDelay(20/portTICK_PERIOD_MS);
            (void) ubx_setval(UBLOX_CFG_PRT_UART1_OUTPROT_UBX, 1, VAL_LAYER_RAM_BBR);
            vTaskDelay(20/portTICK_PERIOD_MS);
            (void) ubx_setval(UBLOX_CFG_PRT_UART1_INPUT_UBX, 1, VAL_LAYER_RAM_BBR);

            ESP_LOGI("UBX_CONNECT","UART Connected");
            int ret_init = ubx_init();

            if (ret_init)
            {
                uart_driver_delete(UBX_UART_NUM);

                const uart_config_t uart_config_new = {
                    .baud_rate = 460800,
                    .data_bits = UART_DATA_8_BITS,
                    .parity = UART_PARITY_DISABLE,
                    .stop_bits = UART_STOP_BITS_1,
                    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
                    .source_clk = UART_SCLK_DEFAULT,
                };
            
                    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_driver_install(UBX_UART_NUM, 257, 0, 0, NULL, 0));
                    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_param_config(UBX_UART_NUM, &uart_config_new));
                    ESP_ERROR_CHECK_WITHOUT_ABORT(uart_set_pin(UBX_UART_NUM, tx, rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
                
            }
            
            return 1;
        }

        else
        {
            uart_driver_delete(UBX_UART_NUM);
            ESP_LOGI("UBX_CONNECT","UART Baudrate Not Found !");
            continue;
        }
        
    }
    return 0; // Return 1 is not triggered thus, connection attempt failed.
}

int ubx_init()
{
    /* Check the default measurement rate (25 hz) is on device. */
    if (ubx_getval(UBLOX_CFG_RATE_MEAS) != 40)
    {
        /* If the measurement rate is not 25hz, that means the device reset internal
        configuration. So the configuration should be reset to desired values */

        /* Ubx configurations*/
        ESP_LOGI("UBX_INIT:","Measurement rate is 10hz. Resetting all settings");
        int ret_gps = ubx_setval(UBLOX_CFG_SIGNAL_GPS_EN, 1, VAL_LAYER_RAM_BBR);       // Enable GPS
        int ret_gal = ubx_setval(UBLOX_CFG_SIGNAL_GALILEO_EN, 1, VAL_LAYER_RAM_BBR);   // Enable Galileo
        int ret_qzss = ubx_setval(UBLOX_CFG_SIGNAL_QZSS_EN, 1, VAL_LAYER_RAM_BBR);      // Enable QZSS
        int ret_sbas = ubx_setval(UBLOX_CFG_SIGNAL_SBAS_EN, 1, VAL_LAYER_RAM_BBR);      // Enable SBAS
        int ret_bdou = ubx_setval(UBLOX_CFG_SIGNAL_BEIDOU_EN, 1, VAL_LAYER_RAM_BBR);    // Enable BeiDou
        int ret_glonsas = ubx_setval(UBLOX_CFG_SIGNAL_GLONASS_EN, 1, VAL_LAYER_RAM_BBR);   // Enable Glonas
        int ret_dyn = ubx_setval(UBLOX_CFG_NAVSPG_DYNMODEL, 4, VAL_LAYER_RAM_BBR);     // Set dynamic model to automotive
        int ret_mrate = ubx_setval(UBLOX_CFG_RATE_MEAS,(uint16_t)40, VAL_LAYER_RAM_BBR);           // Set measurement rate to 25hz
        int ret_nrate = ubx_setval(UBLOX_CFG_RATE_NAV, (uint16_t)1, VAL_LAYER_RAM_BBR);            // Set navigation rate to 1
        int ret_prot = ubx_setval(UBLOX_CFG_PRT_UART1_OUTPROT_UBX, 1, VAL_LAYER_RAM_BBR);
        int ret_baud = ubx_setval(UBLOX_CFG_UART1_BAUDRATE, 460800, VAL_LAYER_RAM_BBR);


        ESP_LOGI("UBX_INIT:", "GPS Enable: %d",ret_gps);
        ESP_LOGI("UBX_INIT:", "Galileo Enable: %d",ret_gal);
        ESP_LOGI("UBX_INIT:", "QZSS Enable: %d",ret_qzss);
        ESP_LOGI("UBX_INIT:", "SBAS Enable: %d",ret_sbas);
        ESP_LOGI("UBX_INIT:", "BeiDou Enable: %d",ret_bdou);
        ESP_LOGI("UBX_INIT:", "Glonsas Enable: %d",ret_glonsas);
        ESP_LOGI("UBX_INIT:", "Dynamic model set: %d",ret_dyn);
        ESP_LOGI("UBX_INIT:", "Measurement rate set: %d",ret_mrate);
        ESP_LOGI("UBX_INIT:", "Navigation rate set: %d",ret_nrate);
        ESP_LOGI("UBX_INIT:", "Protocol set: %d",ret_prot);
        ESP_LOGI("UBX_INIT:", "Baudrate set: %d",ret_baud);

        return 1;
    }

    else 
    {
        /* If the measurement rate is 25 hz, no configuration is needed*/
        return 0;
    }
    

}

int ubx_send_message(struct ubxMessage *msg)
{   
    
    uint8_t *data = (uint8_t *)malloc(msg->len + 8); // 8 for 6 byte header bytes and 2 byte checksum

    data[0] = UBX_SYNC_1;
    data[1] = UBX_SYNC_2;
    data[2] = msg->cls;
    data[3] = msg->id;
    data[4] = msg->len & 0xFF;
    data[5] = msg->len >> 8;

    for (int i = 0; i < msg->len; i++)  // Create payload
    {
        data[i + 6] = msg->payload[i];
    }

    calcChecksum(msg);
    
    data[msg->len + 6] = msg->checksumA;   // Write checksums
    data[msg->len + 7] = msg->checksumB;
    
   
    // Now, the message is ready to be sent.
    int msg_status = uart_write_bytes(UBX_UART_NUM, data, msg->len + 8);

    free(data);

    return (msg_status > 0);
}



int ubx_read_message(uint8_t* buffer) // Checksum feature is not supported currently (!!)
{
    uint8_t header_data[6] = {0};
       
    int read_byte_header = uart_read_bytes(UBX_UART_NUM, header_data, 6, 100/portTICK_PERIOD_MS);
    
    uint16_t payload_size = header_data[5] << 8 | header_data[4]; // two 1 bytes to one 2 byte
   
    int read_byte_payload = uart_read_bytes(UBX_UART_NUM, buffer, payload_size, 100/portTICK_PERIOD_MS);

    uint8_t checksums[2] = {0}; 
    int read_byte_checksum = uart_read_bytes(UBX_UART_NUM, checksums, 2, 100/portTICK_PERIOD_MS);

    // Calculate checksum to validate data

    struct ubxMessage validate;
    validate.cls = header_data[2];
    validate.id = header_data[3];
    validate.len = header_data[5] << 8 | header_data[4];
    validate.payload = (uint8_t*) malloc(validate.len);
    
    for (int i = 0; i < validate.len; i++)
    {
        validate.payload[i] = buffer[i];
    }
    
    calcChecksum(&validate);

    if (!((validate.checksumA == checksums[0]) && (validate.checksumB == checksums[1])))
    {
        ESP_LOGI("CHECKSUM","GPS CHECKSUM FAILED");
        memset(buffer, 0, payload_size);
        free(validate.payload);
        return 0;
    }
    
    free(validate.payload);
    int ret_con = (read_byte_header > 0) * (read_byte_payload == payload_size) * (read_byte_checksum == 2) * (header_data[0] == UBX_SYNC_1) * (header_data[1] == UBX_SYNC_2);
    

    return ret_con;
    
}

int ubx_parse_message(uint8_t class, struct Result *buffer, uint8_t* payload)
{
    switch (class)
    {
    case UBX_NAV_PVT:
        /* This code block will parse the data into meaningful data based on the ubx documentation */
        
        buffer->longtitude[0] = payload[24]; // Byte offsets for payload are based on the documentation 
        buffer->longtitude[1] = payload[25];
        buffer->longtitude[2] = payload[26];
        buffer->longtitude[3] = payload[27];

        buffer->latitude[0] = payload[28];
        buffer->latitude[1] = payload[29];
        buffer->latitude[2] = payload[30];
        buffer->latitude[3] = payload[31];

        buffer->altitude[0] = payload[32];
        buffer->altitude[1] = payload[33];
        buffer->altitude[2] = payload[34];
        buffer->altitude[3] = payload[35];

        buffer->altitude_msl[0] = payload[36];
        buffer->altitude_msl[1] = payload[37];
        buffer->altitude_msl[2] = payload[38];
        buffer->altitude_msl[3] = payload[39];

        buffer->horizontal_acc[0] = payload[40];
        buffer->horizontal_acc[1] = payload[41];
        buffer->horizontal_acc[2] = payload[42];
        buffer->horizontal_acc[3] = payload[43];

        buffer->speed[0] = payload[60];
        buffer->speed[1] = payload[61];
        buffer->speed[2] = payload[62];
        buffer->speed[3] = payload[63];

        buffer->heading[0] = payload[64];
        buffer->heading[1] = payload[65];
        buffer->heading[2] = payload[66];
        buffer->heading[3] = payload[67];

        buffer->speed_acc[0] = payload[68];
        buffer->speed_acc[1] = payload[69];
        buffer->speed_acc[2] = payload[70];
        buffer->speed_acc[3] = payload[71];

        buffer->pdop[0] = payload[76];
        buffer->pdop[1] = payload[77];

        buffer->millisecond[0] = payload[0];
        buffer->millisecond[1] = payload[1];
        buffer->millisecond[2] = payload[2];
        buffer->millisecond[3] = payload[3];

        buffer->numSIV = payload[23];

        buffer->second = payload[10];
        
        buffer->minute = payload[9];

        buffer->fixtype = payload[20];
       

        return 1;
        break;
    
    default:
        break;
    }
    return 0;
}

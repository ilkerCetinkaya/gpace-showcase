

#ifndef UBX_GPACE_H
#define UBX_GPACE_H

#include "driver/uart.h"

//Struct and union definitions

struct ubxMessage
{
    uint8_t cls;
    uint8_t id;
    uint16_t len;          // Length of the payload. Does not include cls, id, or checksum bytes
    uint8_t *payload;     // We will allocate RAM for the payload if/when needed.
    uint8_t checksumA;     // Given to us from module. Checked against the rolling calculated A/B checksums.
    uint8_t checksumB;

};

struct Result
{
    uint8_t longtitude[4];
    uint8_t latitude[4];
    uint8_t altitude[4];
    uint8_t altitude_msl[4];
    uint8_t horizontal_acc[4];
    uint8_t speed[4];
    uint8_t heading[4];
    uint8_t speed_acc[4];
    uint8_t pdop[2];
    uint8_t numSIV;
    uint8_t second;
    uint8_t minute;
    uint8_t fixtype;
    uint8_t millisecond[4];
};


#define UBX_DEBUG 0
#define UBX_UART_NUM UART_NUM_1

#define UBX_SYNC_1 0xB5
#define UBX_SYNC_2 0x62

// Class macros
#define UBX_NAV  0x01  // Navigation Results Messages: Position, Speed, Time, Acceleration, Heading, DOP, SVs used
#define UBX_RXM  0x02  // Receiver Manager Messages: Satellite Status, RTC Status
#define UBX_INF  0x04  // Information Messages: Printf-Style Messages, with IDs such as Error, Warning, Notice
#define UBX_ACK  0x05  // Ack/Nak Messages: Acknowledge or Reject messages to UBX-CFG input messages
#define UBX_CFG  0x06  // Configuration Input Messages: Configure the receiver.
#define UBX_UPD  0x09  // Firmware Update Messages: Memory/Flash erase/write, Reboot, Flash identification, etc.
#define UBX_MON  0x0A  // Monitoring Messages: Communication Status, CPU Load, Stack Usage, Task Status
#define UBX_AID  0x0B  //(NEO-M8P ONLY!!!) AssistNow Aiding Messages: Ephemeris, Almanac, other A-GPS data input
#define UBX_TIM  0x0D  // Timing Messages: Time Pulse Output, Time Mark Results
#define UBX_ESF  0x10  //(NEO-M8P ONLY!!!) External Sensor Fusion Messages: External Sensor Measurements and Status Information
#define UBX_MGA  0x13  // Multiple GNSS Assistance Messages: Assistance data for various GNSS
#define UBX_LOG  0x21  // Logging Messages: Log creation, deletion, info and retrieval
#define UBX_SEC  0x27  // Security Feature Messages
#define UBX_HNR  0x28  //(NEO-M8P ONLY!!!) High Rate Navigation Results Messages: High rate time, position speed, heading

// Message macros (Just NAV and CFG is defined for pre-release)
#define UBX_NAV_ATT  0x05       // Vehicle "Attitude" Solution
#define UBX_NAV_CLOCK  0x22     // Clock Solution
#define UBX_NAV_DOP  0x04      // Dilution of precision
#define UBX_NAV_EOE  0x61      // End of Epoch
#define UBX_NAV_GEOFENCE  0x39  // Geofencing status. Used to poll the geofence status
#define UBX_NAV_HPPOSECEF  0x13 // High Precision Position Solution in ECEF. Used to find our positional accuracy (high precision).
#define UBX_NAV_HPPOSLLH  0x14  // High Precision Geodetic Position Solution. Used for obtaining lat/long/alt in high precision
#define UBX_NAV_ODO  0x09       // Odometer Solution
#define UBX_NAV_ORB  0x34       // GNSS Orbit Database Info
#define UBX_NAV_PL  0x62        // Protection Level Information
#define UBX_NAV_POSECEF  0x01   // Position Solution in ECEF
#define UBX_NAV_POSLLH  0x02    // Geodetic Position Solution
#define UBX_NAV_PVT  0x07       // All the things! Position, velocity, time, PDOP, height, h/v accuracies, number of satellites. Navigation Position Velocity Time Solution.
#define UBX_NAV_PVAT  0x17      // Navigation position velocity attitude time solution (ZED-F9R only)
#define UBX_NAV_RELPOSNED  0x3C // Relative Positioning Information in NED frame
#define UBX_NAV_RESETODO  0x10  // Reset odometer
#define UBX_NAV_SAT  0x35       // Satellite Information
#define UBX_NAV_SIG  0x43       // Signal Information
#define UBX_NAV_STATUS  0x03    // Receiver Navigation Status
#define UBX_NAV_SVIN  0x3B      // Survey-in data. Used for checking Survey In status
#define UBX_NAV_TIMEBDS  0x24   // BDS Time Solution
#define UBX_NAV_TIMEGAL  0x25   // Galileo Time Solution
#define UBX_NAV_TIMEGLO  0x23   // GLO Time Solution
#define UBX_NAV_TIMEGPS  0x20   // GPS Time Solution
#define UBX_NAV_TIMELS  0x26    // Leap second event information
#define UBX_NAV_TIMEUTC  0x21   // UTC Time Solution
#define UBX_NAV_VELECEF  0x11   // Velocity Solution in ECEF
#define UBX_NAV_VELNED  0x12    // Velocity Solution in NED
#define UBX_NAV_AOPSTATUS  0x60 // AssistNow Autonomous status

// Macro to power down gps
#define UBX_RXM_PMREQ 0x41

#define UBX_CFG_ANT  0x13       // Antenna Control Settings. Used to configure the antenna control settings
#define UBX_CFG_BATCH  0x93     // Get/set data batching configuration.
#define UBX_CFG_CFG  0x09       // Clear, Save, and Load Configurations. Used to save current configuration
#define UBX_CFG_DAT  0x06       // Set User-defined Datum or The currently defined Datum
#define UBX_CFG_DGNSS  0x70     // DGNSS configuration
#define UBX_CFG_ESFALG  0x56    // ESF alignment
#define UBX_CFG_ESFA  0x4C      // ESF accelerometer
#define UBX_CFG_ESFG  0x4D      // ESF gyro
#define UBX_CFG_GEOFENCE  0x69  // Geofencing configuration. Used to configure a geofence
#define UBX_CFG_GNSS  0x3E      // GNSS system configuration
#define UBX_CFG_HNR  0x5C       // High Navigation Rate
#define UBX_CFG_INF  0x02       // Depending on packet length, either: poll configuration for one protocol, or information message configuration
#define UBX_CFG_ITFM  0x39      // Jamming/Interference Monitor configuration
#define UBX_CFG_LOGFILTER  0x47 // Data Logger Configuration
#define UBX_CFG_MSG  0x01       // Poll a message configuration, or Set Message Rate(s), or Set Message Rate
#define UBX_CFG_NAV5  0x24      // Navigation Engine Settings. Used to configure the navigation engine including the dynamic model.
#define UBX_CFG_NAVX5  0x23     // Navigation Engine Expert Settings
#define UBX_CFG_NMEA  0x17      // Extended NMEA protocol configuration V1
#define UBX_CFG_ODO  0x1E       // Odometer, Low-speed COG Engine Settings
#define UBX_CFG_PM2  0x3B       // Extended power management configuration
#define UBX_CFG_PMS  0x86       // Power mode setup
#define UBX_CFG_PRT  0x00       // Used to configure port specifics. Polls the configuration for one I/O Port, or Port configuration for UART ports, or Port configuration for USB port, or Port configuration for SPI port, or Port configuration for DDC port
#define UBX_CFG_PWR  0x57       // Put receiver in a defined power state
#define UBX_CFG_RATE  0x08      // Navigation/Measurement Rate Settings. Used to set port baud rates.
#define UBX_CFG_RINV  0x34      // Contents of Remote Inventory
#define UBX_CFG_RST  0x04       // Reset Receiver / Clear Backup Data Structures. Used to reset device.
#define UBX_CFG_RXM  0x11       // RXM configuration
#define UBX_CFG_SBAS  0x16      // SBAS configuration
#define UBX_CFG_TMODE3  0x71    // Time Mode Settings 3. Used to enable Survey In Mode
#define UBX_CFG_TP5  0x31       // Time Pulse Parameters
#define UBX_CFG_USB  0x1B       // USB Configuration
#define UBX_CFG_VALDEL  0x8C    // Used for config of higher version u-blox modules (ie protocol v27 and above). Deletes values corresponding to provided keys/ provided keys with a transaction
#define UBX_CFG_VALGET  0x8B    // Used for config of higher version u-blox modules (ie protocol v27 and above). Configuration Items
#define UBX_CFG_VALSET  0x8A    // Used for config of higher version u-blox modules (ie protocol v27 and above). Sets values corresponding to provided key-value pairs/ provided key-value pairs within a transaction.


// M9N Config headers
#define UBLOX_CFG_SIGNAL_GPS_EN 0x1031001F
#define UBLOX_CFG_SIGNAL_GLONASS_EN 0x10310025
#define UBLOX_CFG_SIGNAL_GALILEO_EN 0x10310021
#define UBLOX_CFG_SIGNAL_BEIDOU_EN 0x10310022
#define UBLOX_CFG_SIGNAL_QZSS_EN 0x10310024
#define UBLOX_CFG_SIGNAL_SBAS_EN 0x10310020
#define UBLOX_CFG_NAVSPG_DYNMODEL 0x20110021
#define UBLOX_CFG_RATE_MEAS 0x30210001
#define UBLOX_CFG_RATE_NAV 0x30210002
#define UBLOX_CFG_PRT_UART1_OUTPROT_UBX 0x10740001
#define UBXLOX_CFG_PRT_UART1_OUTPROT_NMEA 0x10740002
#define UBLOX_CFG_PRT_UART1_INPUT_UBX 0x10730001
#define UBLOX_CFG_UART1_BAUDRATE 0x40520001
#define VAL_LAYER_RAM 0x01
#define VAL_LAYER_BBR 0x02
#define VAL_LAYER_RAM_BBR (VAL_LAYER_RAM | VAL_LAYER_BBR)
#define VAL_RXM_PMREQ_WAKEUPSOURCE_UARTRX 0x00000008  // UART Rx interrupt configuration value


// Communication type (Just for ublox protocol)
#define COM_TYPE_UBX (1 << 0)
#define UBX_DYN_MODEL_AUTOMOTİVE 4


// Function declarations
int ubx_connect(int tx, int rx);
int ubx_init();
int ubx_send_message(struct ubxMessage *msg);
int ubx_read_message(uint8_t* buffer);
int ubx_parse_message(uint8_t class, struct Result * buffer, uint8_t* payload);
void calcChecksum(struct ubxMessage *msg);
int ubx_get_pvt(struct Result *buffer);
int ubx_setval(uint32_t key, uint32_t value, uint8_t layer);
int ubx_getval(uint32_t key);


#endif
#include "lorawan_network.h"
#include "dust_sensor.h"
#include "standby.h"

#define     STANDBY_TIME_S          1 * 60

extern EventQueue ev_queue;

static uint32_t DEVADDR = 0x26011021;
static uint8_t NWKSKEY[] = { 0x10, 0xCA, 0xD6, 0xA0, 0x67, 0x0F, 0xD0, 0x4D, 0x0B, 0x66, 0xC1, 0x52, 0xC7, 0x18, 0x99, 0xDD };
static uint8_t APPSKEY[] = { 0xBE, 0x08, 0xF4, 0x54, 0xCE, 0x75, 0xCD, 0x75, 0x9C, 0x74, 0x66, 0xCE, 0xE2, 0x06, 0xB6, 0x3C };


DustSensor *dust = new DustSensor(D7);

float dust_concentration = 0.0f;
bool dust_updated = false;

void dust_sensor_cb(int lpo, float ratio, float concentration) {
    dust_concentration = concentration;
    dust_updated = true;
}

void check_for_updated_dust() {
    if (dust_updated) {
        dust_updated = false;
        printf("Measured concentration = %.2f pcs/0.01cf\n", dust_concentration);

        CayenneLPP payload(50);
        payload.addAnalogInput(1, dust_concentration / 100.0f); // save space

        if (!lorawan_send(&payload)) {
            delete dust;
            standby(STANDBY_TIME_S);
        }
    }
}

static void lora_event_handler(lorawan_event_t event) {
    switch (event) {
        case CONNECTED:
            printf("[LNWK][INFO] Connection - Successful\n");
            break;
        case DISCONNECTED:
            ev_queue.break_dispatch();
            printf("[LNWK][INFO] Disconnected Successfully\n");
            break;
        case TX_DONE:
            printf("[LNWK][INFO] Message Sent to Network Server\n");

            delete dust;
            standby(STANDBY_TIME_S);
            break;
        case TX_TIMEOUT:
        case TX_ERROR:
        case TX_CRYPTO_ERROR:
        case TX_SCHEDULING_ERROR:
            printf("[LNWK][INFO] Transmission Error - EventCode = %d\n", event);

            delete dust;
            standby(STANDBY_TIME_S);
            break;
        case RX_DONE:
            printf("[LNWK][INFO] Received message from Network Server\n");
            receive_message();
            break;
        case RX_TIMEOUT:
        case RX_ERROR:
            printf("[LNWK][INFO] Error in reception - Code = %d\n", event);
            break;
        case JOIN_FAILURE:
            printf("[LNWK][INFO] OTAA Failed - Check Keys\n");
            break;
        default:
            MBED_ASSERT("Unknown Event");
    }
}

int main() {
    set_time(0);

    printf("\r=======================================\n");
    printf("\r      DeKUT Air Quality Sensor         \n");
    printf("\r=======================================\n");

    lorawan_setup(
        DEVADDR,
        NWKSKEY,
        APPSKEY,
        &lora_event_handler);

    printf("Measuring dust...\n");
    dust->measure(&dust_sensor_cb);

    ev_queue.call_every(3000, &check_for_updated_dust);

    ev_queue.dispatch_forever();
}

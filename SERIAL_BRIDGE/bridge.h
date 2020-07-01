#pragma once

#include <ESPAsyncWebServer.h>
#include "esphome/core/component.h"
#include "esphome/core/controller.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <StreamString.h>
#include <Updater.h>
#include <vector>
#include <FS.h>
#include "esphome/core/helpers.h"
#include <ESPAsyncUDP.h>

using namespace esphome;
using namespace std;
static const char *TAG = "BRIDGE";

#define serialSpeed        (2000000ul)
//#define serialSpeed        (115200ul)
/* Character timeout for end of frame detection */
#define characterBreakTime (5ul + 2*(10000000ul/serialSpeed))
#define ESP_NOW_PACKET_SIZE (100)
/* RX/TX queue size in packets (MUST BE POWER OF 2) */
#define ESP_NOW_QUEUE_SIZE (128)
/* RX/TX queue mask */
#define ESP_NOW_QUEUE_MASK (ESP_NOW_QUEUE_SIZE-1)


//====================================================================================
//===============================--- PACKET QUEUE --==================================
//====================================================================================

/*!
 * \brief Single ESP NOW packet structure.
 */
typedef struct {
	uint8_t buf[ESP_NOW_PACKET_SIZE];           /*!< Packet data            */
	int len;                                    /*!< Packe t size in bytes. */
} esp_now_packet_t;

/*!
 * \brief ESP NOW packet queue.
 */
typedef struct {
	esp_now_packet_t data[ESP_NOW_QUEUE_SIZE];  /*!< Queue data             */
	int rd;                                     /*!< Read pointer           */
	int wr;                                     /*!< Write pointer          */
} esp_now_queue_t;

void esp_now_queue_init(esp_now_queue_t *q) { q->rd = q->wr = 0; }
esp_now_packet_t *esp_now_queue_get_rx(esp_now_queue_t *q) {return &q->data[q->rd];}
esp_now_packet_t *esp_now_queue_get_tx(esp_now_queue_t *q) {return &q->data[q->wr];}
void esp_now_queue_pull(esp_now_queue_t *q) {int rd = q->rd; rd++; rd&=ESP_NOW_QUEUE_MASK; q->rd=rd;}
void esp_now_queue_tx_commit(esp_now_queue_t *q) {int wr = q->wr; wr++; wr&=ESP_NOW_QUEUE_MASK; q->wr=wr;}
#define esp_now_queue_not_empty(q) ((q)->rd != (q)->wr)
void esp_now_queue_push(esp_now_queue_t *q,uint8_t *data, uint8_t len )
{
	esp_now_packet_t *p = esp_now_queue_get_tx(q);
	memcpy(p->buf, data, len);
	p->len = len;
	esp_now_queue_tx_commit(q);
}
//====================================================================================


class BRIDGE;

class BRIDGE: public Component, public UARTDevice {
public:
	BRIDGE(UARTComponent *parent) : UARTDevice(parent) {}

	void init() {}
	//==========================================================================================
	
	void deinit() {}
	//==========================================================================================
	
	void setup() override {
		previousMicros = 0;          /*!< Previous micros for timeout computations      */
		lastBytes      = 0;          /*!< Last avail bytes for timeout computations     */
		if (udp.listen(1234)) {
			ESP_LOGD(TAG, "UDP Listening...");
			udp.onPacket([this](AsyncUDPPacket packet) {
				if (packet.length() <= ESP_NOW_PACKET_SIZE) {
					esp_now_queue_push(&rx_queue, packet.data(), packet.length());
					m_addr = packet.remoteIP();
					m_port = packet.remotePort();
				}
			});
		}
	}
	//==========================================================================================

	/*!
	 * \brief The main loop.
	 * 1) Send packets from RX queue over uart port.
	 * 2) Collect data from uart and push it to TX queue.
	 */
	void loop() override {
		esp_now_packet_t *p;
		uint64_t currentMicros;
		int avail;
		
		/* Send data from RX queue */
		if (esp_now_queue_not_empty(&rx_queue)) {
			p = esp_now_queue_get_rx(&rx_queue);
			if (Serial.availableForWrite() >= p->len) {
				Serial.write(p->buf, p->len);
				esp_now_queue_pull(&rx_queue);
			}
			
		}

		/* Collect data and push it to TX queue */
		p             = nullptr;
		currentMicros = micros64();
		avail         = Serial.available();
		if (avail) {
			if (avail < ESP_NOW_PACKET_SIZE) {
				if (avail > lastBytes) {
					previousMicros = currentMicros;
					lastBytes      = avail;
				} else if ((currentMicros - previousMicros) > characterBreakTime) {
					/* Transmision break timeout  */
					p = &tx_packet;
				}
			} else {
				/* Buffer is full */
				p = &tx_packet;
			}
			/* Read data from serial */
			if (p) {
				p->len = Serial.read((char *)p->buf, (size_t)ESP_NOW_PACKET_SIZE);
				lastBytes = 0;
			}
		}
		if (p) {
			udp.writeTo(p->buf, p->len, m_addr, m_port);
			previousMicros = currentMicros;
		}
	}
	//==========================================================================================
public:
	AsyncUDP         udp;
	esp_now_packet_t tx_packet;               /*!< Transmit queue                                */
	esp_now_queue_t  rx_queue;                /*!< Receive queue                                 */
	IPAddress        m_addr;
	uint16_t         m_port;
	uint64_t         previousMicros;          /*!< Previous micros for timeout computations      */
	int              lastBytes;               /*!< Last avail bytes for timeout computations     */
};



/*
 * CDC-ACM (USB serial) for STM32F411 Black Pill.
 * Host sees /dev/ttyACM0; bytes received are echoed back.
 */

#include <string.h>

#include <libopencm3/usb/usbd.h>
#include <libopencm3/usb/cdc.h>
#include <libopencm3/usb/dwc/otg_fs.h>

#include "cdcacm.h"

#define CDC_EP_DATA_RX	0x01
#define CDC_EP_DATA_TX	0x82
#define CDC_EP_IN_NUM	2
#define CDC_PACKET_SIZE	64
#define CDC_RX_SIZE	256

static usbd_device *g_usbd;
static volatile int configured;
static uint8_t rx_buf[CDC_RX_SIZE];
static volatile unsigned rx_head;
static volatile unsigned rx_tail;

static const struct usb_device_descriptor dev = {
	.bLength = USB_DT_DEVICE_SIZE,
	.bDescriptorType = USB_DT_DEVICE,
	.bcdUSB = 0x0200,
	.bDeviceClass = USB_CLASS_CDC,
	.bDeviceSubClass = 0,
	.bDeviceProtocol = 0,
	.bMaxPacketSize0 = 64,
	.idVendor = 0x0483,
	.idProduct = 0x5740,
	.bcdDevice = 0x0200,
	.iManufacturer = 1,
	.iProduct = 2,
	.iSerialNumber = 3,
	.bNumConfigurations = 1,
};

/*
 * Notification endpoint is unused, but Linux cdc_acm expects it to exist.
 */
static const struct usb_endpoint_descriptor comm_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x83,
	.bmAttributes = USB_ENDPOINT_ATTR_INTERRUPT,
	.wMaxPacketSize = 16,
	.bInterval = 255,
}};

static const struct usb_endpoint_descriptor data_endp[] = {{
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x01,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}, {
	.bLength = USB_DT_ENDPOINT_SIZE,
	.bDescriptorType = USB_DT_ENDPOINT,
	.bEndpointAddress = 0x82,
	.bmAttributes = USB_ENDPOINT_ATTR_BULK,
	.wMaxPacketSize = 64,
	.bInterval = 1,
}};

static const struct {
	struct usb_cdc_header_descriptor header;
	struct usb_cdc_call_management_descriptor call_mgmt;
	struct usb_cdc_acm_descriptor acm;
	struct usb_cdc_union_descriptor cdc_union;
} __attribute__((packed)) cdcacm_functional_descriptors = {
	.header = {
		.bFunctionLength = sizeof(struct usb_cdc_header_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_HEADER,
		.bcdCDC = 0x0110,
	},
	.call_mgmt = {
		.bFunctionLength = sizeof(struct usb_cdc_call_management_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_CALL_MANAGEMENT,
		.bmCapabilities = 0,
		.bDataInterface = 1,
	},
	.acm = {
		.bFunctionLength = sizeof(struct usb_cdc_acm_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_ACM,
		.bmCapabilities = 0,
	},
	.cdc_union = {
		.bFunctionLength = sizeof(struct usb_cdc_union_descriptor),
		.bDescriptorType = CS_INTERFACE,
		.bDescriptorSubtype = USB_CDC_TYPE_UNION,
		.bControlInterface = 0,
		.bSubordinateInterface0 = 1,
	},
};

static const struct usb_interface_descriptor comm_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 0,
	.bAlternateSetting = 0,
	.bNumEndpoints = 1,
	.bInterfaceClass = USB_CLASS_CDC,
	.bInterfaceSubClass = USB_CDC_SUBCLASS_ACM,
	.bInterfaceProtocol = USB_CDC_PROTOCOL_AT,
	.iInterface = 0,
	.endpoint = comm_endp,
	.extra = &cdcacm_functional_descriptors,
	.extralen = sizeof(cdcacm_functional_descriptors),
}};

static const struct usb_interface_descriptor data_iface[] = {{
	.bLength = USB_DT_INTERFACE_SIZE,
	.bDescriptorType = USB_DT_INTERFACE,
	.bInterfaceNumber = 1,
	.bAlternateSetting = 0,
	.bNumEndpoints = 2,
	.bInterfaceClass = USB_CLASS_DATA,
	.bInterfaceSubClass = 0,
	.bInterfaceProtocol = 0,
	.iInterface = 0,
	.endpoint = data_endp,
}};

static const struct usb_interface ifaces[] = {{
	.num_altsetting = 1,
	.altsetting = comm_iface,
}, {
	.num_altsetting = 1,
	.altsetting = data_iface,
}};

static const struct usb_config_descriptor config = {
	.bLength = USB_DT_CONFIGURATION_SIZE,
	.bDescriptorType = USB_DT_CONFIGURATION,
	.wTotalLength = 0,
	.bNumInterfaces = 2,
	.bConfigurationValue = 1,
	.iConfiguration = 0,
	.bmAttributes = 0x80,
	.bMaxPower = 0x32,
	.interface = ifaces,
};

static const char *usb_strings[] = {
	"Test-libopencm3",
	"Black Pill USB Serial",
	"F411CEU6",
};

static uint8_t usbd_control_buffer[128];

static struct usb_cdc_line_coding line_coding = {
	.dwDTERate = 115200,
	.bCharFormat = USB_CDC_1_STOP_BITS,
	.bParityType = USB_CDC_NO_PARITY,
	.bDataBits = 8,
};

static enum usbd_request_return_codes cdcacm_control_request(
	usbd_device *usbd_dev, struct usb_setup_data *req, uint8_t **buf,
	uint16_t *len, void (**complete)(usbd_device *usbd_dev,
					 struct usb_setup_data *req))
{
	(void)complete;
	(void)usbd_dev;

	switch (req->bRequest) {
	case USB_CDC_REQ_SET_CONTROL_LINE_STATE:
		/*
		 * Linux cdc_acm requires this even though it is optional
		 * and we do not advertise it. picocom asserts DTR here.
		 */
		return USBD_REQ_HANDLED;
	case USB_CDC_REQ_SET_LINE_CODING:
		if (*len < sizeof(line_coding)) {
			return USBD_REQ_NOTSUPP;
		}
		memcpy(&line_coding, *buf, sizeof(line_coding));
		return USBD_REQ_HANDLED;
	case USB_CDC_REQ_GET_LINE_CODING:
		if (*len > sizeof(line_coding)) {
			*len = sizeof(line_coding);
		}
		memcpy(*buf, &line_coding, *len);
		return USBD_REQ_HANDLED;
	default:
		return USBD_REQ_NOTSUPP;
	}
}

static unsigned rx_next(unsigned i)
{
	return (i + 1U) % CDC_RX_SIZE;
}

static void cdcacm_data_rx_cb(usbd_device *usbd_dev, uint8_t ep)
{
	char pkt[CDC_PACKET_SIZE];
	int len;
	int i;

	(void)ep;
	len = usbd_ep_read_packet(usbd_dev, CDC_EP_DATA_RX, pkt, CDC_PACKET_SIZE);
	for (i = 0; i < len; i++) {
		unsigned next = rx_next(rx_head);
		if (next == rx_tail) {
			break;
		}
		rx_buf[rx_head] = (uint8_t)pkt[i];
		rx_head = next;
	}
}

static void cdcacm_set_config(usbd_device *usbd_dev, uint16_t wValue)
{
	(void)wValue;

	usbd_ep_setup(usbd_dev, CDC_EP_DATA_RX, USB_ENDPOINT_ATTR_BULK,
		      CDC_PACKET_SIZE, cdcacm_data_rx_cb);
	usbd_ep_setup(usbd_dev, CDC_EP_DATA_TX, USB_ENDPOINT_ATTR_BULK,
		      CDC_PACKET_SIZE, NULL);
	usbd_ep_setup(usbd_dev, 0x83, USB_ENDPOINT_ATTR_INTERRUPT, 16, NULL);
	configured = 1;

	usbd_register_control_callback(
		usbd_dev,
		USB_REQ_TYPE_CLASS | USB_REQ_TYPE_INTERFACE,
		USB_REQ_TYPE_TYPE | USB_REQ_TYPE_RECIPIENT,
		cdcacm_control_request);
}

static void cdcacm_disable_vbus_sense(void)
{
	/*
	 * WeAct Black Pill does not wire USB VBUS to PA9. Without this the
	 * OTG core waits forever for VBUS and the host never enumerates.
	 */
	if (OTG_FS_CID >= OTG_CID_HAS_VBDEN) {
		OTG_FS_GCCFG &= ~OTG_GCCFG_VBDEN;
	} else {
		OTG_FS_GCCFG |= OTG_GCCFG_NOVBUSSENS;
		OTG_FS_GCCFG &= ~(OTG_GCCFG_VBUSBSEN | OTG_GCCFG_VBUSASEN);
	}
}

void cdcacm_init(void)
{
	g_usbd = usbd_init(&otgfs_usb_driver, &dev, &config, usb_strings, 3,
			   usbd_control_buffer, sizeof(usbd_control_buffer));
	cdcacm_disable_vbus_sense();
	usbd_register_set_config_callback(g_usbd, cdcacm_set_config);
}

void cdcacm_poll(void)
{
	if (g_usbd) {
		usbd_poll(g_usbd);
	}
}

static int cdcacm_tx_idle(void)
{
	/* Do not call usbd_ep_write_packet() while EPENA is set: the DWC
	 * driver then flushes the TX FIFO and can wait forever on EPDISD.
	 */
	return (OTG_FS_DIEPCTL(CDC_EP_IN_NUM) & OTG_DIEPCTL0_EPENA) == 0;
}

static int cdcacm_wait_tx_idle(void)
{
	int tries = 0;

	while (!cdcacm_tx_idle()) {
		usbd_poll(g_usbd);
		if (++tries > 20000) {
			return 0;
		}
	}
	return 1;
}

int cdcacm_write(const void *data, size_t len)
{
	const uint8_t *p = data;
	size_t sent = 0;

	if (!g_usbd || !configured || data == NULL || len == 0) {
		return 0;
	}

	while (sent < len) {
		uint16_t chunk = (uint16_t)(len - sent);
		uint16_t wrote;

		if (chunk > CDC_PACKET_SIZE) {
			chunk = CDC_PACKET_SIZE;
		}
		if (!cdcacm_wait_tx_idle()) {
			return (int)sent;
		}

		wrote = usbd_ep_write_packet(g_usbd, CDC_EP_DATA_TX, p + sent, chunk);
		if (wrote == 0) {
			return (int)sent;
		}
		sent += wrote;
	}

	return (int)sent;
}

int cdcacm_print(const char *s)
{
	if (s == NULL) {
		return 0;
	}
	return cdcacm_write(s, strlen(s));
}

int cdcacm_puts(const char *s)
{
	char line[80];
	size_t n;

	if (s == NULL) {
		return 0;
	}

	n = strlen(s);
	if (n > sizeof(line) - 2) {
		n = sizeof(line) - 2;
	}
	memcpy(line, s, n);
	line[n++] = '\r';
	line[n++] = '\n';
	return cdcacm_write(line, n);
}

int cdcacm_read(void *data, size_t maxlen)
{
	uint8_t *out = data;
	size_t n = 0;

	if (data == NULL || maxlen == 0) {
		return 0;
	}

	while (n < maxlen && rx_tail != rx_head) {
		out[n++] = rx_buf[rx_tail];
		rx_tail = rx_next(rx_tail);
	}

	return (int)n;
}

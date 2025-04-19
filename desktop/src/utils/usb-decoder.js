/**
 * USBShark - Military-grade USB protocol analyzer
 * USB Decoder Utility
 * 
 * Provides functions for decoding USB packets and protocol elements
 */

/**
 * USB Packet ID (PID) definitions
 */
const PID = {
    // Token PIDs
    OUT: 0xE1,
    IN: 0x69,
    SOF: 0xA5,
    SETUP: 0x2D,
    
    // Data PIDs
    DATA0: 0xC3,
    DATA1: 0x4B,
    DATA2: 0x87,
    MDATA: 0x0F,
    
    // Handshake PIDs
    ACK: 0xD2,
    NAK: 0x5A,
    STALL: 0x1E,
    NYET: 0x96,
    
    // Special PIDs
    PRE: 0x3C,
    ERR: 0x3C,  // Same as PRE
    SPLIT: 0x78,
    PING: 0xB4,
    RESERVED: 0xF0
};

/**
 * PID type groups
 */
const PID_TYPES = {
    'Token': [PID.OUT, PID.IN, PID.SOF, PID.SETUP, PID.PING],
    'Data': [PID.DATA0, PID.DATA1, PID.DATA2, PID.MDATA],
    'Handshake': [PID.ACK, PID.NAK, PID.STALL, PID.NYET],
    'Special': [PID.PRE, PID.ERR, PID.SPLIT, PID.RESERVED]
};

/**
 * USB Standard Request Codes
 */
const REQUEST_CODES = {
    GET_STATUS: 0,
    CLEAR_FEATURE: 1,
    SET_FEATURE: 3,
    SET_ADDRESS: 5,
    GET_DESCRIPTOR: 6,
    SET_DESCRIPTOR: 7,
    GET_CONFIGURATION: 8,
    SET_CONFIGURATION: 9,
    GET_INTERFACE: 10,
    SET_INTERFACE: 11,
    SYNCH_FRAME: 12
};

/**
 * USB Descriptor Types
 */
const DESCRIPTOR_TYPES = {
    DEVICE: 1,
    CONFIGURATION: 2,
    STRING: 3,
    INTERFACE: 4,
    ENDPOINT: 5,
    DEVICE_QUALIFIER: 6,
    OTHER_SPEED_CONFIGURATION: 7,
    INTERFACE_POWER: 8,
    HID: 0x21,
    REPORT: 0x22,
    PHYSICAL: 0x23,
    HUB: 0x29
};

/**
 * Request Type bit masks
 */
const REQUEST_TYPE = {
    DIRECTION_MASK: 0x80,
    TYPE_MASK: 0x60,
    RECIPIENT_MASK: 0x1F,
    
    // Directions
    HOST_TO_DEVICE: 0x00,
    DEVICE_TO_HOST: 0x80,
    
    // Types
    STANDARD: 0x00,
    CLASS: 0x20,
    VENDOR: 0x40,
    
    // Recipients
    DEVICE: 0x00,
    INTERFACE: 0x01,
    ENDPOINT: 0x02,
    OTHER: 0x03
};

/**
 * Get the name of a PID from its value
 * @param {number} pid The PID value
 * @returns {string} The PID name or 'Unknown'
 */
function getPidName(pid) {
    for (const [name, value] of Object.entries(PID)) {
        if (value === pid) {
            return name;
        }
    }
    return 'Unknown';
}

/**
 * Get the packet type from a PID
 * @param {number} pid The PID value
 * @returns {string} The packet type or 'Unknown'
 */
function getPacketType(pid) {
    for (const [type, pids] of Object.entries(PID_TYPES)) {
        if (pids.includes(pid)) {
            return type;
        }
    }
    return 'Unknown';
}

/**
 * Decode a USB setup packet
 * @param {Uint8Array} data The setup packet data (8 bytes)
 * @returns {Object} Decoded setup packet information
 */
function decodeSetupPacket(data) {
    if (!data || data.length < 8) {
        return { error: 'Invalid setup packet data' };
    }
    
    const bmRequestType = data[0];
    const bRequest = data[1];
    const wValue = data[2] | (data[3] << 8);
    const wIndex = data[4] | (data[5] << 8);
    const wLength = data[6] | (data[7] << 8);
    
    // Determine direction
    const direction = (bmRequestType & REQUEST_TYPE.DIRECTION_MASK) ?
        'Device-to-Host' : 'Host-to-Device';
    
    // Determine request type
    let type;
    switch (bmRequestType & REQUEST_TYPE.TYPE_MASK) {
        case REQUEST_TYPE.STANDARD:
            type = 'Standard';
            break;
        case REQUEST_TYPE.CLASS:
            type = 'Class';
            break;
        case REQUEST_TYPE.VENDOR:
            type = 'Vendor';
            break;
        default:
            type = 'Reserved';
    }
    
    // Determine recipient
    let recipient;
    switch (bmRequestType & REQUEST_TYPE.RECIPIENT_MASK) {
        case REQUEST_TYPE.DEVICE:
            recipient = 'Device';
            break;
        case REQUEST_TYPE.INTERFACE:
            recipient = `Interface ${wIndex & 0xFF}`;
            break;
        case REQUEST_TYPE.ENDPOINT:
            const epNum = wIndex & 0x0F;
            const epDir = (wIndex & 0x80) ? 'IN' : 'OUT';
            recipient = `Endpoint ${epNum} (${epDir})`;
            break;
        case REQUEST_TYPE.OTHER:
            recipient = 'Other';
            break;
        default:
            recipient = 'Reserved';
    }
    
    // Get standard request name and details if applicable
    let requestName = 'Unknown Request';
    let requestDetails = '';
    
    if ((bmRequestType & REQUEST_TYPE.TYPE_MASK) === REQUEST_TYPE.STANDARD) {
        // Handle standard requests
        switch (bRequest) {
            case REQUEST_CODES.GET_STATUS:
                requestName = 'GET_STATUS';
                break;
                
            case REQUEST_CODES.CLEAR_FEATURE:
                requestName = 'CLEAR_FEATURE';
                requestDetails = `Feature ID: ${wValue}`;
                break;
                
            case REQUEST_CODES.SET_FEATURE:
                requestName = 'SET_FEATURE';
                requestDetails = `Feature ID: ${wValue}`;
                break;
                
            case REQUEST_CODES.SET_ADDRESS:
                requestName = 'SET_ADDRESS';
                requestDetails = `Device Address: ${wValue}`;
                break;
                
            case REQUEST_CODES.GET_DESCRIPTOR:
                requestName = 'GET_DESCRIPTOR';
                const descriptorType = (wValue >> 8);
                const descriptorIndex = (wValue & 0xFF);
                
                let descriptorName = 'Unknown';
                for (const [name, value] of Object.entries(DESCRIPTOR_TYPES)) {
                    if (value === descriptorType) {
                        descriptorName = name;
                        break;
                    }
                }
                
                requestDetails = `${descriptorName} Descriptor, Index ${descriptorIndex}, Length ${wLength}`;
                break;
                
            case REQUEST_CODES.SET_DESCRIPTOR:
                requestName = 'SET_DESCRIPTOR';
                break;
                
            case REQUEST_CODES.GET_CONFIGURATION:
                requestName = 'GET_CONFIGURATION';
                break;
                
            case REQUEST_CODES.SET_CONFIGURATION:
                requestName = 'SET_CONFIGURATION';
                requestDetails = `Configuration Value: ${wValue}`;
                break;
                
            case REQUEST_CODES.GET_INTERFACE:
                requestName = 'GET_INTERFACE';
                requestDetails = `Interface: ${wIndex}`;
                break;
                
            case REQUEST_CODES.SET_INTERFACE:
                requestName = 'SET_INTERFACE';
                requestDetails = `Interface: ${wIndex}, Alternate Setting: ${wValue}`;
                break;
                
            case REQUEST_CODES.SYNCH_FRAME:
                requestName = 'SYNCH_FRAME';
                break;
                
            default:
                requestName = `Unknown Standard Request (${bRequest})`;
        }
    } else if ((bmRequestType & REQUEST_TYPE.TYPE_MASK) === REQUEST_TYPE.CLASS) {
        requestName = `Class Request (${bRequest})`;
        
        // Handle common class requests (e.g., HID, CDC, etc.)
        if ((bmRequestType & REQUEST_TYPE.RECIPIENT_MASK) === REQUEST_TYPE.INTERFACE) {
            // Try to determine interface class based on bInterfaceClass
            // This would require maintaining interface class information
            requestDetails = `Interface ${wIndex}, Value: 0x${wValue.toString(16).padStart(4, '0')}`;
        }
    } else if ((bmRequestType & REQUEST_TYPE.TYPE_MASK) === REQUEST_TYPE.VENDOR) {
        requestName = `Vendor Request (${bRequest})`;
        requestDetails = `Value: 0x${wValue.toString(16).padStart(4, '0')}, Index: 0x${wIndex.toString(16).padStart(4, '0')}`;
    }
    
    return {
        bmRequestType,
        bRequest,
        wValue,
        wIndex,
        wLength,
        direction,
        type,
        recipient,
        requestName,
        requestDetails
    };
}

/**
 * Format a data buffer as a hexdump with ASCII representation
 * @param {Uint8Array} data The data buffer
 * @returns {string} Formatted hexdump string
 */
function formatHexDump(data) {
    if (!data || !data.length) {
        return 'No data';
    }
    
    const bytesPerLine = 16;
    let result = '';
    
    for (let offset = 0; offset < data.length; offset += bytesPerLine) {
        // Address column
        result += `${offset.toString(16).padStart(4, '0')}: `;
        
        // Hex columns
        let hexPart = '';
        let asciiPart = '';
        
        for (let i = 0; i < bytesPerLine; i++) {
            if (offset + i < data.length) {
                const byte = data[offset + i];
                
                // Hex representation
                hexPart += byte.toString(16).padStart(2, '0') + ' ';
                
                // ASCII representation (only printable characters)
                if (byte >= 32 && byte <= 126) {
                    asciiPart += String.fromCharCode(byte);
                } else {
                    asciiPart += '.';
                }
            } else {
                // Padding for last line if needed
                hexPart += '   ';
                asciiPart += ' ';
            }
            
            // Group bytes for readability
            if (i === 7) {
                hexPart += ' ';
            }
        }
        
        result += hexPart + ' |' + asciiPart + '|\n';
    }
    
    return result;
}

/**
 * Parse USB device address and endpoint from token packet data
 * @param {Uint8Array} data The token packet data
 * @returns {Object} Device address and endpoint information
 */
function parseDeviceAddressAndEndpoint(data) {
    if (!data || data.length < 2) {
        return { error: 'Invalid token packet data' };
    }
    
    const deviceAddress = data[0] & 0x7F;
    const endpoint = ((data[1] & 0x7) << 1) | ((data[0] & 0x80) >> 7);
    
    return {
        deviceAddress,
        endpoint
    };
}

/**
 * Get friendly name for a Request Type
 * @param {number} bmRequestType The request type byte
 * @returns {string} Human-readable request type
 */
function getRequestTypeName(bmRequestType) {
    // Direction
    const direction = (bmRequestType & REQUEST_TYPE.DIRECTION_MASK) ? 
        'Device-to-Host' : 'Host-to-Device';
    
    // Type
    let type;
    switch (bmRequestType & REQUEST_TYPE.TYPE_MASK) {
        case REQUEST_TYPE.STANDARD: type = 'Standard'; break;
        case REQUEST_TYPE.CLASS: type = 'Class'; break;
        case REQUEST_TYPE.VENDOR: type = 'Vendor'; break;
        default: type = 'Reserved';
    }
    
    // Recipient
    let recipient;
    switch (bmRequestType & REQUEST_TYPE.RECIPIENT_MASK) {
        case REQUEST_TYPE.DEVICE: recipient = 'Device'; break;
        case REQUEST_TYPE.INTERFACE: recipient = 'Interface'; break;
        case REQUEST_TYPE.ENDPOINT: recipient = 'Endpoint'; break;
        case REQUEST_TYPE.OTHER: recipient = 'Other'; break;
        default: recipient = 'Reserved';
    }
    
    return `${direction}, ${type}, ${recipient}`;
}

/**
 * Parse SOF packet frame number
 * @param {Uint8Array} data The SOF packet data
 * @returns {Object} Frame number information
 */
function parseSOFFrameNumber(data) {
    if (!data || data.length < 2) {
        return { error: 'Invalid SOF packet data' };
    }
    
    // Frame number is 11 bits
    const frameNumber = (data[0] | (data[1] << 8)) & 0x07FF;
    
    return {
        frameNumber
    };
}

module.exports = {
    PID,
    PID_TYPES,
    REQUEST_CODES,
    DESCRIPTOR_TYPES,
    REQUEST_TYPE,
    getPidName,
    getPacketType,
    decodeSetupPacket,
    formatHexDump,
    parseDeviceAddressAndEndpoint,
    getRequestTypeName,
    parseSOFFrameNumber
}; 
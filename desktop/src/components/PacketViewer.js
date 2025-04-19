/**
 * USBShark - Military-grade USB protocol analyzer
 * Packet Viewer Component
 */

const usbDecoder = require('../utils/usb-decoder');

class PacketViewer {
    constructor(containerElement) {
        this.container = containerElement;
        this.selectedPacket = null;
        this.selectedTransaction = null;
        this.transactions = [];
        
        // Create packet details elements
        this.detailsContainer = document.createElement('div');
        this.detailsContainer.className = 'packet-details';
        
        this.headerDiv = document.createElement('div');
        this.headerDiv.className = 'packet-header';
        this.detailsContainer.appendChild(this.headerDiv);
        
        this.timestampDiv = document.createElement('div');
        this.timestampDiv.className = 'packet-timestamp';
        this.headerDiv.appendChild(this.timestampDiv);
        
        this.packetTypeDiv = document.createElement('div');
        this.packetTypeDiv.className = 'packet-type';
        this.headerDiv.appendChild(this.packetTypeDiv);
        
        this.rawDataDiv = document.createElement('div');
        this.rawDataDiv.className = 'packet-raw-data';
        this.detailsContainer.appendChild(this.rawDataDiv);
        
        this.decodedDiv = document.createElement('div');
        this.decodedDiv.className = 'packet-decoded';
        this.detailsContainer.appendChild(this.decodedDiv);
        
        this.container.appendChild(this.detailsContainer);
        
        // Initially hide details
        this.clearDetails();
    }
    
    /**
     * Clear packet details view
     */
    clearDetails() {
        this.headerDiv.style.display = 'none';
        this.rawDataDiv.style.display = 'none';
        this.decodedDiv.style.display = 'none';
        this.selectedPacket = null;
        this.selectedTransaction = null;
    }
    
    /**
     * Show details for a specific packet
     * @param {Object} packet Packet object to display
     */
    showPacketDetails(packet) {
        this.selectedPacket = packet;
        this.selectedTransaction = null;
        
        // Show packet header
        this.headerDiv.style.display = 'flex';
        this.timestampDiv.textContent = `Timestamp: ${new Date(packet.timestamp).toISOString().substr(11, 12)}`;
        
        // Get packet type and PID name
        const pidName = usbDecoder.getPidName(packet.pid);
        const packetType = usbDecoder.getPacketType(packet.pid);
        this.packetTypeDiv.textContent = `${packetType} Packet - PID: ${pidName} (0x${packet.pid.toString(16).toUpperCase()})`;
        
        // Show raw data if available
        if (packet.data && packet.data.length > 0) {
            this.rawDataDiv.style.display = 'block';
            this.rawDataDiv.innerHTML = `
                <h3>Raw Data (${packet.data.length} bytes)</h3>
                <div class="hex-dump">${usbDecoder.formatHexData(packet.data)}</div>
            `;
        } else {
            this.rawDataDiv.style.display = 'none';
        }
        
        // Show decoded data based on packet type
        this.decodedDiv.style.display = 'block';
        this.decodedDiv.innerHTML = '<h3>Decoded Fields</h3>';
        
        const fieldsTable = document.createElement('table');
        fieldsTable.className = 'decoded-fields';
        
        // Add appropriate fields based on packet type
        if (packetType === 'Token') {
            // For token packets, show address and endpoint
            if (packet.data && packet.data.length >= 2) {
                const address = packet.data[0] & 0x7F;
                const endpoint = ((packet.data[1] & 0x7) << 1) | ((packet.data[0] & 0x80) >> 7);
                
                this.addTableRow(fieldsTable, 'Device Address', `${address} (0x${address.toString(16).toUpperCase()})`);
                this.addTableRow(fieldsTable, 'Endpoint', `${endpoint} (0x${endpoint.toString(16).toUpperCase()})`);
            }
        } else if (pidName === 'SETUP' && packet.data) {
            // For SETUP packets, attempt to decode setup packet if associated data is available
            // (This would typically be in the DATA0 packet following the SETUP token)
            // Just showing the token fields for now
            if (packet.data && packet.data.length >= 2) {
                const address = packet.data[0] & 0x7F;
                const endpoint = ((packet.data[1] & 0x7) << 1) | ((packet.data[0] & 0x80) >> 7);
                
                this.addTableRow(fieldsTable, 'Device Address', `${address} (0x${address.toString(16).toUpperCase()})`);
                this.addTableRow(fieldsTable, 'Endpoint', `${endpoint} (0x${endpoint.toString(16).toUpperCase()})`);
            }
        } else if (pidName === 'DATA0' || pidName === 'DATA1') {
            // For data packets, try to determine the context
            // Check if this is part of a setup transaction
            const transaction = this.findTransactionForPacket(packet);
            if (transaction && transaction.type === 'Control Setup') {
                const setupData = usbDecoder.decodeSetupPacket(packet.data);
                
                this.addTableRow(fieldsTable, 'Request Type', `0x${setupData.bmRequestType.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Direction', setupData.direction);
                this.addTableRow(fieldsTable, 'Type', setupData.type);
                this.addTableRow(fieldsTable, 'Recipient', setupData.recipient);
                this.addTableRow(fieldsTable, 'Request', setupData.requestName);
                this.addTableRow(fieldsTable, 'Value', `0x${setupData.wValue.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Index', `0x${setupData.wIndex.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Length', setupData.wLength);
                
                if (setupData.requestDetails) {
                    this.addTableRow(fieldsTable, 'Details', setupData.requestDetails);
                }
            } else if (packet.data.length === 18 && packet.data[1] === 0x01) {
                // This might be a device descriptor
                const desc = usbDecoder.decodeDeviceDescriptor(packet.data);
                
                this.addTableRow(fieldsTable, 'Descriptor Type', 'Device');
                this.addTableRow(fieldsTable, 'USB Version', `${(desc.bcdUSB >> 8) & 0xFF}.${(desc.bcdUSB & 0xFF)}`);
                this.addTableRow(fieldsTable, 'Device Class', `0x${desc.bDeviceClass.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Vendor ID', `0x${desc.idVendor.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Product ID', `0x${desc.idProduct.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Device Version', `${(desc.bcdDevice >> 8) & 0xFF}.${(desc.bcdDevice & 0xFF)}`);
            } else if (packet.data.length >= 9 && packet.data[1] === 0x02) {
                // This might be a configuration descriptor
                const desc = usbDecoder.decodeConfigDescriptor(packet.data);
                
                this.addTableRow(fieldsTable, 'Descriptor Type', 'Configuration');
                this.addTableRow(fieldsTable, 'Total Length', desc.wTotalLength);
                this.addTableRow(fieldsTable, 'Num Interfaces', desc.bNumInterfaces);
                this.addTableRow(fieldsTable, 'Config Value', desc.bConfigurationValue);
                this.addTableRow(fieldsTable, 'Attributes', `0x${desc.bmAttributes.toString(16).toUpperCase()}`);
                this.addTableRow(fieldsTable, 'Max Power', `${desc.bMaxPower * 2}mA`);
            }
        }
        
        // Add fields table to decoded div if we have rows
        if (fieldsTable.rows.length > 0) {
            this.decodedDiv.appendChild(fieldsTable);
        } else {
            this.decodedDiv.innerHTML += '<p>No additional decoded fields available for this packet type.</p>';
        }
    }
    
    /**
     * Show details for a transaction (group of related packets)
     * @param {Object} transaction Transaction object
     */
    showTransactionDetails(transaction) {
        this.selectedTransaction = transaction;
        this.selectedPacket = null;
        
        // Show header
        this.headerDiv.style.display = 'flex';
        this.timestampDiv.textContent = `Timestamp: ${new Date(transaction.packets[0].timestamp).toISOString().substr(11, 12)}`;
        this.packetTypeDiv.textContent = `Transaction: ${transaction.type}`;
        
        // No raw data for transactions
        this.rawDataDiv.style.display = 'none';
        
        // Show decoded transaction info
        this.decodedDiv.style.display = 'block';
        this.decodedDiv.innerHTML = '<h3>Transaction Summary</h3>';
        
        const fieldsTable = document.createElement('table');
        fieldsTable.className = 'decoded-fields';
        
        this.addTableRow(fieldsTable, 'Type', transaction.type);
        this.addTableRow(fieldsTable, 'Description', transaction.description);
        this.addTableRow(fieldsTable, 'Packets', transaction.packets.length);
        
        // List the packets in this transaction
        const packetList = document.createElement('div');
        packetList.className = 'transaction-packets';
        packetList.innerHTML = '<h3>Packets in Transaction</h3><ul></ul>';
        
        const ul = packetList.querySelector('ul');
        transaction.packets.forEach((packet, index) => {
            const li = document.createElement('li');
            li.textContent = `${index + 1}. ${usbDecoder.getPidName(packet.pid)}`;
            li.dataset.packetIndex = index;
            li.className = 'packet-list-item';
            
            // Add click handler to show individual packet
            li.addEventListener('click', () => this.showPacketDetails(packet));
            
            ul.appendChild(li);
        });
        
        this.decodedDiv.appendChild(fieldsTable);
        this.decodedDiv.appendChild(packetList);
    }
    
    /**
     * Add a row to a table with key-value pair
     * @param {HTMLTableElement} table Table to add row to
     * @param {string} key Field name
     * @param {string} value Field value
     */
    addTableRow(table, key, value) {
        const row = table.insertRow();
        const keyCell = row.insertCell(0);
        const valueCell = row.insertCell(1);
        
        keyCell.textContent = key;
        valueCell.textContent = value;
    }
    
    /**
     * Find the transaction containing a specific packet
     * @param {Object} packet The packet to find
     * @return {Object|null} The transaction or null if not found
     */
    findTransactionForPacket(packet) {
        for (const transaction of this.transactions) {
            if (transaction.packets.includes(packet)) {
                return transaction;
            }
        }
        return null;
    }
    
    /**
     * Update the transactions list
     * @param {Array<Object>} transactions List of transaction objects
     */
    setTransactions(transactions) {
        this.transactions = transactions;
    }
}

module.exports = PacketViewer; 
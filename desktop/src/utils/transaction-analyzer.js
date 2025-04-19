/**
 * USBShark - Military-grade USB protocol analyzer
 * Transaction Analyzer Module
 * 
 * Groups USB packets into logical transactions (Setup, IN, OUT, etc.)
 */

const usbDecoder = require('./usb-decoder');

/**
 * Transaction Analyzer class
 * Groups USB packets into logical transactions
 */
class TransactionAnalyzer {
    constructor() {
        this.transactions = [];
        this.pendingPackets = [];
        this.pendingTransaction = null;
    }

    /**
     * Reset the analyzer state
     */
    reset() {
        this.transactions = [];
        this.pendingPackets = [];
        this.pendingTransaction = null;
    }

    /**
     * Process a new packet and update transactions
     * @param {Object} packet The packet to process
     * @returns {Array} Array of completed transactions (if any)
     */
    processPacket(packet) {
        // Make sure packet has required fields
        if (!packet || typeof packet.pid !== 'number') {
            console.error('Invalid packet format:', packet);
            return [];
        }

        const completedTransactions = [];
        const pidName = usbDecoder.getPidName(packet.pid);
        const packetType = usbDecoder.getPacketType(packet.pid);

        // If this is a SOF packet, process it separately (not part of a transaction)
        if (pidName === 'SOF') {
            const sofTransaction = {
                type: 'Start-of-Frame',
                description: 'USB Start of Frame marker',
                packets: [packet],
                timestamp: packet.timestamp,
                frameNumber: packet.data ? (packet.data[0] | (packet.data[1] << 8)) & 0x07FF : null
            };
            completedTransactions.push(sofTransaction);
            return completedTransactions;
        }

        // Check if this packet starts a new transaction
        if (this.shouldStartNewTransaction(packet, pidName, packetType)) {
            // Flush any pending transaction as incomplete
            if (this.pendingTransaction && this.pendingPackets.length > 0) {
                this.pendingTransaction.packets = [...this.pendingPackets];
                this.pendingTransaction.description += ' (incomplete)';
                completedTransactions.push(this.pendingTransaction);
            }

            // Start a new transaction
            this.pendingPackets = [packet];
            this.pendingTransaction = this.createNewTransaction(packet, pidName, packetType);
        } else {
            // Add to current transaction
            this.pendingPackets.push(packet);
            
            // Check if transaction is complete
            if (this.isTransactionComplete(pidName, packetType)) {
                this.pendingTransaction.packets = [...this.pendingPackets];
                
                // Add any additional analysis
                this.enhanceTransaction(this.pendingTransaction);
                
                completedTransactions.push(this.pendingTransaction);
                this.pendingPackets = [];
                this.pendingTransaction = null;
            }
        }

        return completedTransactions;
    }

    /**
     * Determine if a packet should start a new transaction
     * @param {Object} packet The packet
     * @param {string} pidName PID name 
     * @param {string} packetType Packet type
     * @returns {boolean} True if packet should start new transaction
     */
    shouldStartNewTransaction(packet, pidName, packetType) {
        // Always start a new transaction if we don't have a pending one
        if (!this.pendingTransaction) {
            return true;
        }

        // If this is a token packet, it's the start of a new transaction
        if (packetType === 'Token') {
            return true;
        }

        // Special case: If we have a pending SETUP and this is DATA0, it's part of the same transaction
        if (this.pendingTransaction.type === 'Control Setup' && 
            this.pendingPackets.length === 1 &&
            pidName === 'DATA0') {
            return false;
        }

        // If we have a pending IN/OUT token and this is DATA/ACK/NAK/STALL, it's part of the same transaction
        if ((this.pendingTransaction.type === 'IN Transaction' || 
            this.pendingTransaction.type === 'OUT Transaction') && 
            (packetType === 'Data' || packetType === 'Handshake')) {
            return false;
        }

        // Default: start a new transaction
        return true;
    }

    /**
     * Create a new transaction based on the first packet
     * @param {Object} packet First packet in transaction
     * @param {string} pidName PID name
     * @param {string} packetType Packet type
     * @returns {Object} New transaction object
     */
    createNewTransaction(packet, pidName, packetType) {
        if (packetType === 'Token') {
            // Extract device address and endpoint if available
            let deviceAddress = null;
            let endpoint = null;
            
            if (packet.data && packet.data.length >= 2) {
                deviceAddress = packet.data[0] & 0x7F;
                endpoint = ((packet.data[1] & 0x7) << 1) | ((packet.data[0] & 0x80) >> 7);
            }
            
            // Create transaction based on token type
            switch (pidName) {
                case 'SETUP':
                    return {
                        type: 'Control Setup',
                        description: `Setup transaction to device ${deviceAddress}, endpoint ${endpoint}`,
                        deviceAddress,
                        endpoint,
                        timestamp: packet.timestamp
                    };
                case 'IN':
                    return {
                        type: 'IN Transaction',
                        description: `IN transaction from device ${deviceAddress}, endpoint ${endpoint}`,
                        deviceAddress,
                        endpoint,
                        timestamp: packet.timestamp
                    };
                case 'OUT':
                    return {
                        type: 'OUT Transaction',
                        description: `OUT transaction to device ${deviceAddress}, endpoint ${endpoint}`,
                        deviceAddress,
                        endpoint,
                        timestamp: packet.timestamp
                    };
                case 'PING':
                    return {
                        type: 'PING Transaction',
                        description: `PING to device ${deviceAddress}, endpoint ${endpoint}`,
                        deviceAddress,
                        endpoint,
                        timestamp: packet.timestamp
                    };
                default:
                    return {
                        type: `${pidName} Transaction`,
                        description: `Transaction starting with ${pidName}`,
                        timestamp: packet.timestamp
                    };
            }
        } else {
            // Non-token packets shouldn't normally start transactions, but handle anyway
            return {
                type: `${pidName} Packet`,
                description: `Single ${pidName} packet (not part of standard transaction)`,
                timestamp: packet.timestamp
            };
        }
    }

    /**
     * Determine if the current transaction is complete
     * @param {string} pidName PID name of the most recent packet
     * @param {string} packetType Packet type of the most recent packet
     * @returns {boolean} True if transaction is complete
     */
    isTransactionComplete(pidName, packetType) {
        if (!this.pendingTransaction) {
            return false;
        }

        // A transaction is typically complete when it receives a handshake packet
        if (packetType === 'Handshake') {
            return true;
        }

        // Control Setup transactions should have at least 3 packets (SETUP, DATA0, ACK)
        if (this.pendingTransaction.type === 'Control Setup') {
            // If we have a DATA0 packet as the second packet, we need a handshake as third
            if (this.pendingPackets.length === 2 && 
                usbDecoder.getPidName(this.pendingPackets[1].pid) === 'DATA0') {
                return false;
            }
            
            // If we have 3 or more packets, the transaction is complete
            return this.pendingPackets.length >= 3;
        }

        // IN transactions need at least 2 packets (IN token + DATA/NAK/STALL)
        if (this.pendingTransaction.type === 'IN Transaction') {
            // If the second packet is DATA0/1, we need a handshake as third
            if (this.pendingPackets.length === 2 && 
                (usbDecoder.getPidName(this.pendingPackets[1].pid) === 'DATA0' || 
                 usbDecoder.getPidName(this.pendingPackets[1].pid) === 'DATA1')) {
                return false;
            }
            
            // Otherwise, if we have 2 or more packets, the transaction is complete
            return this.pendingPackets.length >= 2;
        }

        // OUT transactions need at least 3 packets (OUT token + DATA0/1 + ACK/NAK/STALL)
        if (this.pendingTransaction.type === 'OUT Transaction') {
            return this.pendingPackets.length >= 3;
        }

        // Default: transaction is incomplete
        return false;
    }

    /**
     * Add additional analysis to a completed transaction
     * @param {Object} transaction The transaction to enhance
     */
    enhanceTransaction(transaction) {
        if (transaction.type === 'Control Setup' && transaction.packets.length >= 2) {
            // Get the DATA0 packet that should follow the SETUP
            const dataPacket = transaction.packets.find(p => 
                usbDecoder.getPidName(p.pid) === 'DATA0');
                
            if (dataPacket && dataPacket.data) {
                const setupData = usbDecoder.decodeSetupPacket(dataPacket.data);
                transaction.setupData = setupData;
                
                // Update description with request details
                transaction.description = `${setupData.requestName} (${setupData.type})`;
                
                if (setupData.requestDetails) {
                    transaction.description += `: ${setupData.requestDetails}`;
                }
            }
        }
        
        // Add status based on handshake
        const lastPacket = transaction.packets[transaction.packets.length - 1];
        const lastPidName = usbDecoder.getPidName(lastPacket.pid);
        
        if (lastPidName === 'ACK') {
            transaction.status = 'Success';
        } else if (lastPidName === 'NAK') {
            transaction.status = 'Not Ready';
        } else if (lastPidName === 'STALL') {
            transaction.status = 'Error: Stalled';
        }
    }

    /**
     * Process multiple packets at once
     * @param {Array} packets Array of packets to process
     * @returns {Array} Array of completed transactions
     */
    processPackets(packets) {
        let allTransactions = [];
        
        for (const packet of packets) {
            const completedTransactions = this.processPacket(packet);
            allTransactions = allTransactions.concat(completedTransactions);
        }
        
        // Flush any pending transaction at the end
        if (this.pendingTransaction && this.pendingPackets.length > 0) {
            this.pendingTransaction.packets = [...this.pendingPackets];
            this.pendingTransaction.description += ' (incomplete)';
            allTransactions.push(this.pendingTransaction);
            this.pendingPackets = [];
            this.pendingTransaction = null;
        }
        
        return allTransactions;
    }

    /**
     * Get all transactions processed so far
     * @returns {Array} Array of all transactions
     */
    getAllTransactions() {
        return [...this.transactions];
    }
}

module.exports = TransactionAnalyzer; 
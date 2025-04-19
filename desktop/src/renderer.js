const { ipcRenderer } = require('electron');

// DOM elements
const connectBtn = document.getElementById('connect-btn');
const disconnectBtn = document.getElementById('disconnect-btn');
const startCaptureBtn = document.getElementById('start-capture-btn');
const stopCaptureBtn = document.getElementById('stop-capture-btn');
const clearDataBtn = document.getElementById('clear-data-btn');
const applyFiltersBtn = document.getElementById('apply-filters-btn');
const statusIndicator = document.getElementById('status-indicator');
const statusText = document.getElementById('status-text');
const packetTableBody = document.getElementById('packet-table-body');
const packetCountEl = document.getElementById('packet-count');
const deviceCountEl = document.getElementById('device-count');
const bufferUsageEl = document.getElementById('buffer-usage');
const elapsedTimeEl = document.getElementById('elapsed-time');
const tabButtons = document.querySelectorAll('.tab-btn');
const tabPanes = document.querySelectorAll('.tab-pane');
const detailsContent = document.getElementById('details-content');
const detailsPlaceholder = document.getElementById('details-placeholder');
const packetInfo = document.getElementById('packet-info');
const packetFields = document.getElementById('packet-fields');
const hexOffset = document.getElementById('hex-offset');
const hexValues = document.getElementById('hex-values');
const hexAscii = document.getElementById('hex-ascii');
const transactionContainer = document.getElementById('transaction-container');

// Modal elements
const connectionModal = document.getElementById('connection-modal');
const portSelect = document.getElementById('port-select');
const baudRateSelect = document.getElementById('baud-rate');
const refreshPortsBtn = document.getElementById('refresh-ports-btn');
const connectModalBtn = document.getElementById('connect-modal-btn');
const cancelModalBtn = document.getElementById('cancel-modal-btn');
const closeModalBtn = document.querySelector('.close-modal');

// State variables
let packetData = [];
let selectedPacketIndex = -1;
let captureStartTime = null;
let elapsedTimeInterval = null;
let transactions = [];
let deviceStatus = {
    connected: false,
    capturing: false,
    deviceCount: 0,
    bufferUsage: 0
};

// USB PID lookups
const PID_NAMES = {
    0xE1: 'OUT',
    0x69: 'IN',
    0xA5: 'SOF',
    0x2D: 'SETUP',
    0xC3: 'DATA0',
    0x4B: 'DATA1',
    0x87: 'DATA2',
    0x0F: 'MDATA',
    0xD2: 'ACK',
    0x5A: 'NAK',
    0x1E: 'STALL',
    0x96: 'NYET',
    0x3C: 'PRE/ERR',
    0x78: 'SPLIT',
    0xB4: 'PING',
    0xF0: 'RESERVED'
};

// Initialize Application
function init() {
    bindEventListeners();
    updateUIState();
}

// Bind Event Listeners
function bindEventListeners() {
    // Button event listeners
    connectBtn.addEventListener('click', showConnectionModal);
    disconnectBtn.addEventListener('click', disconnectDevice);
    startCaptureBtn.addEventListener('click', startCapture);
    stopCaptureBtn.addEventListener('click', stopCapture);
    clearDataBtn.addEventListener('click', clearData);
    applyFiltersBtn.addEventListener('click', applyFilters);
    
    // Modal event listeners
    refreshPortsBtn.addEventListener('click', refreshPorts);
    connectModalBtn.addEventListener('click', connectToDevice);
    cancelModalBtn.addEventListener('click', hideConnectionModal);
    closeModalBtn.addEventListener('click', hideConnectionModal);
    
    // Tab switching
    tabButtons.forEach(button => {
        button.addEventListener('click', () => switchTab(button.dataset.tab));
    });
    
    // IPC event listeners
    ipcRenderer.on('menu:show-connection-dialog', showConnectionModal);
    ipcRenderer.on('menu:start-capture', startCapture);
    ipcRenderer.on('menu:stop-capture', stopCapture);
    ipcRenderer.on('menu:export-data', exportData);
    ipcRenderer.on('device:connected', handleDeviceConnected);
    ipcRenderer.on('device:disconnected', handleDeviceDisconnected);
    ipcRenderer.on('device:connection-error', handleConnectionError);
    ipcRenderer.on('device:error', handleDeviceError);
    ipcRenderer.on('capture:started', handleCaptureStarted);
    ipcRenderer.on('capture:stopped', handleCaptureStopped);
    ipcRenderer.on('capture:error', handleCaptureError);
    ipcRenderer.on('packet:received', handlePacketReceived);
}

// UI State Management
function updateUIState() {
    // Update connection status
    statusIndicator.className = deviceStatus.capturing ? 'capturing' : 
                               (deviceStatus.connected ? 'connected' : 'disconnected');
    statusText.textContent = deviceStatus.capturing ? 'Capturing' :
                            (deviceStatus.connected ? 'Connected' : 'Disconnected');
    
    // Update button states
    connectBtn.disabled = deviceStatus.connected;
    disconnectBtn.disabled = !deviceStatus.connected;
    startCaptureBtn.disabled = !deviceStatus.connected || deviceStatus.capturing;
    stopCaptureBtn.disabled = !deviceStatus.connected || !deviceStatus.capturing;
    
    // Update statistics
    deviceCountEl.textContent = deviceStatus.deviceCount;
    bufferUsageEl.textContent = `${deviceStatus.bufferUsage}%`;
    packetCountEl.textContent = packetData.length;
}

// Tab Switching
function switchTab(tabId) {
    tabButtons.forEach(btn => {
        btn.classList.toggle('active', btn.dataset.tab === tabId);
    });
    
    tabPanes.forEach(pane => {
        pane.classList.toggle('active', pane.id === tabId);
    });
}

// Connection Modal Functions
async function showConnectionModal() {
    await refreshPorts();
    connectionModal.classList.add('show');
}

function hideConnectionModal() {
    connectionModal.classList.remove('show');
}

async function refreshPorts() {
    try {
        const ports = await ipcRenderer.invoke('serial:scan-ports');
        portSelect.innerHTML = '<option value="">Select a port</option>';
        
        ports.forEach(port => {
            const option = document.createElement('option');
            option.value = port.path;
            option.textContent = `${port.path} - ${port.manufacturer || 'Unknown'}`;
            portSelect.appendChild(option);
        });
    } catch (err) {
        console.error('Error scanning ports:', err);
    }
}

function connectToDevice() {
    const port = portSelect.value;
    const baudRate = baudRateSelect.value;
    
    if (!port) {
        alert('Please select a port');
        return;
    }
    
    ipcRenderer.send('serial:connect', { port, baudRate });
    hideConnectionModal();
}

function disconnectDevice() {
    ipcRenderer.send('serial:disconnect');
}

// Capture Control Functions
function startCapture() {
    const config = {
        dataLength: 8,
        data: buildCaptureConfig()
    };
    
    ipcRenderer.send('capture:start', config);
}

function stopCapture() {
    ipcRenderer.send('capture:stop');
}

function buildCaptureConfig() {
    // Build config data based on filter settings
    const config = {
        speed: 1, // 0 = Low speed, 1 = Full speed
        capture_control: document.getElementById('filter-control').checked ? 1 : 0,
        capture_bulk: document.getElementById('filter-bulk').checked ? 1 : 0,
        capture_interrupt: document.getElementById('filter-interrupt').checked ? 1 : 0,
        capture_isoc: document.getElementById('filter-isochronous').checked ? 1 : 0,
        addr_filter: document.getElementById('filter-address-enabled').checked ? 
                    parseInt(document.getElementById('filter-address').value) : 0,
        ep_filter: document.getElementById('filter-endpoint-enabled').checked ? 
                 parseInt(document.getElementById('filter-endpoint').value) : 0,
        filter_in: 0,
        filter_out: 0
    };
    
    // Convert to array of bytes
    return [
        config.speed,
        config.capture_control,
        config.capture_bulk,
        config.capture_interrupt,
        config.capture_isoc,
        config.addr_filter,
        config.ep_filter,
        config.filter_in,
        config.filter_out
    ];
}

function applyFilters() {
    if (deviceStatus.connected && deviceStatus.capturing) {
        stopCapture();
        setTimeout(() => {
            startCapture();
        }, 500);
    }
}

// Data Management Functions
function clearData() {
    packetData = [];
    transactions = [];
    selectedPacketIndex = -1;
    packetTableBody.innerHTML = '';
    detailsContent.hidden = true;
    detailsPlaceholder.hidden = false;
    hexOffset.innerHTML = '<div class="hex-header">Offset</div>';
    hexValues.innerHTML = '<div class="hex-header">Hex Values</div>';
    hexAscii.innerHTML = '<div class="hex-header">ASCII</div>';
    transactionContainer.innerHTML = '';
    packetCountEl.textContent = '0';
}

function exportData() {
    if (packetData.length === 0) {
        alert('No data to export');
        return;
    }
    
    // Implementation could be added here
    alert('Export functionality not yet implemented');
}

// Event Handlers
function handleDeviceConnected(event, port) {
    deviceStatus.connected = true;
    updateUIState();
}

function handleDeviceDisconnected() {
    deviceStatus.connected = false;
    deviceStatus.capturing = false;
    updateUIState();
    
    if (elapsedTimeInterval) {
        clearInterval(elapsedTimeInterval);
        elapsedTimeInterval = null;
    }
}

function handleConnectionError(event, message) {
    alert(`Connection error: ${message}`);
    deviceStatus.connected = false;
    updateUIState();
}

function handleDeviceError(event, message) {
    console.error(`Device error: ${message}`);
    // Could add more error handling here
}

function handleCaptureStarted() {
    deviceStatus.capturing = true;
    captureStartTime = Date.now();
    updateUIState();
    
    // Start elapsed time counter
    if (elapsedTimeInterval) {
        clearInterval(elapsedTimeInterval);
    }
    
    elapsedTimeInterval = setInterval(() => {
        if (captureStartTime) {
            const elapsed = Math.floor((Date.now() - captureStartTime) / 1000);
            const hours = Math.floor(elapsed / 3600).toString().padStart(2, '0');
            const minutes = Math.floor((elapsed % 3600) / 60).toString().padStart(2, '0');
            const seconds = (elapsed % 60).toString().padStart(2, '0');
            elapsedTimeEl.textContent = `${hours}:${minutes}:${seconds}`;
        }
    }, 1000);
}

function handleCaptureStopped() {
    deviceStatus.capturing = false;
    updateUIState();
    
    if (elapsedTimeInterval) {
        clearInterval(elapsedTimeInterval);
        elapsedTimeInterval = null;
    }
}

function handleCaptureError(event, message) {
    alert(`Capture error: ${message}`);
    deviceStatus.capturing = false;
    updateUIState();
    
    if (elapsedTimeInterval) {
        clearInterval(elapsedTimeInterval);
        elapsedTimeInterval = null;
    }
}

function handlePacketReceived(event, packet) {
    // Process different packet types
    switch (packet.type) {
        case 'USB_PACKET':
            processUsbPacket(packet);
            break;
        case 'STATE_CHANGE':
            processStateChange(packet);
            break;
        case 'STATUS_REPORT':
            processStatusReport(packet);
            break;
        case 'ERROR_REPORT':
            processErrorReport(packet);
            break;
        default:
            // Ignore other packet types for now
            break;
    }
}

// Packet Processing Functions
function processUsbPacket(packet) {
    const packetInfo = packet.data;
    
    // Add to packet data array
    packetData.push({
        index: packetData.length,
        timestamp: packetInfo.timestamp,
        pid: packetInfo.pid,
        pidName: PID_NAMES[packetInfo.pid] || `Unknown (0x${packetInfo.pid.toString(16)})`,
        devAddr: packetInfo.devAddr,
        endpoint: packetInfo.endpoint,
        crcValid: packetInfo.crcValid,
        data: packetInfo.data,
        rawPacket: packet
    });
    
    // Add to packet table
    addPacketToTable(packetData[packetData.length - 1]);
    
    // Update transaction view
    updateTransactionView(packetData[packetData.length - 1]);
    
    // Update UI
    packetCountEl.textContent = packetData.length;
}

function processStateChange(packet) {
    const stateInfo = packet.data;
    
    // Log state change
    console.log(`Device state changed: ${stateInfo.state} ${stateInfo.speed || ''}`);
    
    // Update device count if connected
    if (stateInfo.state === 'CONNECTED') {
        deviceStatus.deviceCount++;
    } else if (stateInfo.state === 'DISCONNECTED') {
        deviceStatus.deviceCount = Math.max(0, deviceStatus.deviceCount - 1);
    }
    
    updateUIState();
}

function processStatusReport(packet) {
    const statusInfo = packet.data;
    
    // Update device status
    deviceStatus.deviceCount = statusInfo.deviceCount;
    deviceStatus.bufferUsage = statusInfo.bufferUsage;
    
    updateUIState();
}

function processErrorReport(packet) {
    const errorInfo = packet.data;
    
    // Log error
    console.error(`Device error: ${errorInfo.errorCode} (context: ${errorInfo.context})`);
    
    // Could add more error handling here
}

// UI Update Functions
function addPacketToTable(packet) {
    const row = document.createElement('tr');
    row.dataset.index = packet.index;
    
    row.innerHTML = `
        <td>${packet.index}</td>
        <td>${formatTimestamp(packet.timestamp)}</td>
        <td>${packet.devAddr}</td>
        <td>${packet.endpoint}</td>
        <td>${getPacketType(packet.pid)}</td>
        <td>${packet.pidName}</td>
        <td>${packet.data.length}</td>
        <td>${packet.crcValid ? 'Valid' : 'Error'}</td>
    `;
    
    row.addEventListener('click', () => {
        // Remove selection from previous row
        const selectedRow = packetTableBody.querySelector('.selected');
        if (selectedRow) {
            selectedRow.classList.remove('selected');
        }
        
        // Add selection to this row
        row.classList.add('selected');
        
        // Update packet details
        selectedPacketIndex = packet.index;
        updatePacketDetails(packet);
    });
    
    packetTableBody.appendChild(row);
    
    // Auto-scroll to bottom if near the bottom
    const scrollContainer = packetTableBody.parentElement;
    if (scrollContainer.scrollTop + scrollContainer.clientHeight > scrollContainer.scrollHeight - 50) {
        scrollContainer.scrollTop = scrollContainer.scrollHeight;
    }
}

function updatePacketDetails(packet) {
    // Show details content
    detailsContent.hidden = false;
    detailsPlaceholder.hidden = true;
    
    // Update packet info section
    packetInfo.innerHTML = `
        <div><strong>Index:</strong> ${packet.index}</div>
        <div><strong>Timestamp:</strong> ${formatTimestamp(packet.timestamp)}</div>
        <div><strong>PID:</strong> ${packet.pidName} (0x${packet.pid.toString(16).toUpperCase()})</div>
        <div><strong>Device Address:</strong> ${packet.devAddr}</div>
        <div><strong>Endpoint:</strong> ${packet.endpoint}</div>
        <div><strong>Transfer Type:</strong> ${getPacketType(packet.pid)}</div>
        <div><strong>CRC Status:</strong> ${packet.crcValid ? 'Valid' : 'Invalid'}</div>
        <div><strong>Data Length:</strong> ${packet.data.length} bytes</div>
    `;
    
    // Update packet fields section
    if (packet.data.length > 0) {
        let fieldsHtml = '<table><tr><th>Offset</th><th>Value</th><th>Description</th></tr>';
        
        // Different fields based on packet type
        if (packet.pid === 0x2D && packet.data.length >= 8) { // SETUP packet
            fieldsHtml += `
                <tr><td>0</td><td>0x${packet.data[0].toString(16).padStart(2, '0')}</td><td>bmRequestType</td></tr>
                <tr><td>1</td><td>0x${packet.data[1].toString(16).padStart(2, '0')}</td><td>bRequest</td></tr>
                <tr><td>2-3</td><td>0x${(packet.data[2] | (packet.data[3] << 8)).toString(16).padStart(4, '0')}</td><td>wValue</td></tr>
                <tr><td>4-5</td><td>0x${(packet.data[4] | (packet.data[5] << 8)).toString(16).padStart(4, '0')}</td><td>wIndex</td></tr>
                <tr><td>6-7</td><td>0x${(packet.data[6] | (packet.data[7] << 8)).toString(16).padStart(4, '0')}</td><td>wLength</td></tr>
            `;
        } else {
            // Generic data bytes
            for (let i = 0; i < packet.data.length; i++) {
                fieldsHtml += `
                    <tr>
                        <td>${i}</td>
                        <td>0x${packet.data[i].toString(16).padStart(2, '0')}</td>
                        <td>Data byte ${i}</td>
                    </tr>
                `;
            }
        }
        
        fieldsHtml += '</table>';
        packetFields.innerHTML = fieldsHtml;
    } else {
        packetFields.innerHTML = '<p>No data payload</p>';
    }
    
    // Update hex viewer
    updateHexViewer(packet.data);
}

function updateHexViewer(data) {
    hexOffset.innerHTML = '<div class="hex-header">Offset</div>';
    hexValues.innerHTML = '<div class="hex-header">Hex Values</div>';
    hexAscii.innerHTML = '<div class="hex-header">ASCII</div>';
    
    if (data.length === 0) {
        return;
    }
    
    for (let i = 0; i < data.length; i += 16) {
        const offset = document.createElement('div');
        offset.textContent = i.toString(16).padStart(8, '0');
        hexOffset.appendChild(offset);
        
        const hexRow = document.createElement('div');
        const asciiRow = document.createElement('div');
        
        for (let j = 0; j < 16; j++) {
            if (i + j < data.length) {
                const byte = data[i + j];
                const hex = byte.toString(16).padStart(2, '0');
                const ascii = byte >= 32 && byte <= 126 ? String.fromCharCode(byte) : '.';
                
                hexRow.appendChild(document.createTextNode(hex + ' '));
                asciiRow.appendChild(document.createTextNode(ascii));
            }
        }
        
        hexValues.appendChild(hexRow);
        hexAscii.appendChild(asciiRow);
    }
}

function updateTransactionView(packet) {
    // Implement transaction grouping logic
    // This would group related packets (e.g., SETUP -> DATA -> ACK)
}

// Utility Functions
function formatTimestamp(timestamp) {
    const microseconds = timestamp % 1000;
    const milliseconds = Math.floor(timestamp / 1000) % 1000;
    const seconds = Math.floor(timestamp / 1000000);
    return `${seconds}.${milliseconds.toString().padStart(3, '0')}.${microseconds.toString().padStart(3, '0')}`;
}

function getPacketType(pid) {
    if (pid === 0xE1 || pid === 0x69 || pid === 0xA5 || pid === 0x2D) {
        return 'Token';
    } else if (pid === 0xC3 || pid === 0x4B || pid === 0x87 || pid === 0x0F) {
        return 'Data';
    } else if (pid === 0xD2 || pid === 0x5A || pid === 0x1E || pid === 0x96) {
        return 'Handshake';
    } else {
        return 'Special';
    }
}

// Initialize the application
document.addEventListener('DOMContentLoaded', init); 
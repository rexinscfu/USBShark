const { app, BrowserWindow, ipcMain, Menu } = require('electron');
const path = require('path');
const Store = require('electron-store');
const { SerialPort } = require('serialport');

const store = new Store();

let mainWindow;
let serialConnection = null;
let deviceConnected = false;
let captureActive = false;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1200,
    height: 800,
    minWidth: 800,
    minHeight: 600,
    webPreferences: {
      nodeIntegration: true,
      contextIsolation: false,
      enableRemoteModule: true
    },
    icon: path.join(__dirname, 'assets/icon.png'),
    title: 'USBShark - Military-grade USB Protocol Analyzer'
  });

  mainWindow.loadFile(path.join(__dirname, 'index.html'));
  
  if (process.env.NODE_ENV === 'development') {
    mainWindow.webContents.openDevTools();
  }
  
  createMenu();
}

function createMenu() {
  const template = [
    {
      label: 'File',
      submenu: [
        { 
          label: 'Connect Device', 
          click: () => mainWindow.webContents.send('menu:show-connection-dialog'),
          accelerator: 'CmdOrCtrl+K'
        },
        { type: 'separator' },
        { 
          label: 'Start Capture', 
          click: () => mainWindow.webContents.send('menu:start-capture'),
          enabled: deviceConnected && !captureActive,
          accelerator: 'CmdOrCtrl+R'
        },
        { 
          label: 'Stop Capture', 
          click: () => mainWindow.webContents.send('menu:stop-capture'),
          enabled: deviceConnected && captureActive,
          accelerator: 'CmdOrCtrl+S'
        },
        { type: 'separator' },
        { 
          label: 'Export Data',
          click: () => mainWindow.webContents.send('menu:export-data'),
          accelerator: 'CmdOrCtrl+E'
        },
        { type: 'separator' },
        { role: 'quit' }
      ]
    },
    {
      label: 'View',
      submenu: [
        { role: 'reload' },
        { type: 'separator' },
        { role: 'toggledevtools' },
        { type: 'separator' },
        { role: 'resetzoom' },
        { role: 'zoomin' },
        { role: 'zoomout' },
        { type: 'separator' },
        { role: 'togglefullscreen' }
      ]
    }
  ];
  
  const menu = Menu.buildFromTemplate(template);
  Menu.setApplicationMenu(menu);
}

function updateMenu() {
  createMenu();
}

async function scanPorts() {
  try {
    const ports = await SerialPort.list();
    return ports;
  } catch (err) {
    console.error('Error scanning for serial ports:', err);
    return [];
  }
}

function connectToDevice(port, baudRate) {
  try {
    serialConnection = new SerialPort({
      path: port,
      baudRate: parseInt(baudRate, 10),
      autoOpen: false
    });

    serialConnection.open((err) => {
      if (err) {
        mainWindow.webContents.send('device:connection-error', err.message);
        return;
      }
      
      deviceConnected = true;
      updateMenu();
      mainWindow.webContents.send('device:connected', port);
      
      serialConnection.on('data', (data) => {
        processIncomingData(data);
      });
      
      serialConnection.on('error', (err) => {
        mainWindow.webContents.send('device:error', err.message);
      });
      
      serialConnection.on('close', () => {
        deviceConnected = false;
        updateMenu();
        mainWindow.webContents.send('device:disconnected');
      });
    });
  } catch (err) {
    mainWindow.webContents.send('device:connection-error', err.message);
  }
}

function disconnectFromDevice() {
  if (serialConnection && serialConnection.isOpen) {
    serialConnection.close();
    deviceConnected = false;
    captureActive = false;
    updateMenu();
  }
}

function startCapture(config) {
  if (!deviceConnected || !serialConnection || !serialConnection.isOpen) {
    mainWindow.webContents.send('capture:error', 'No device connected');
    return;
  }
  
  try {
    const startCmd = Buffer.from([0xAA, 0x02, config.dataLength, 0x00, ...config.data]);
    serialConnection.write(startCmd);
    captureActive = true;
    updateMenu();
    mainWindow.webContents.send('capture:started');
  } catch (err) {
    mainWindow.webContents.send('capture:error', err.message);
  }
}

function stopCapture() {
  if (!deviceConnected || !serialConnection || !serialConnection.isOpen) {
    return;
  }
  
  try {
    const stopCmd = Buffer.from([0xAA, 0x03, 0x00, 0x00]);
    serialConnection.write(stopCmd);
    captureActive = false;
    updateMenu();
    mainWindow.webContents.send('capture:stopped');
  } catch (err) {
    mainWindow.webContents.send('capture:error', err.message);
  }
}

const PACKET_TYPES = {
  0x80: 'USB_PACKET',
  0x81: 'STATE_CHANGE',
  0x82: 'STATUS_REPORT',
  0x83: 'ERROR_REPORT',
  0x84: 'BUFFER_OVERFLOW',
  0x85: 'DEV_DESCRIPTOR',
  0x86: 'CONFIG_DESCRIPTOR',
  0x87: 'STRING_DESCRIPTOR'
};

let packetBuffer = Buffer.alloc(0);
let expectedPacketLength = 0;
let parsingHeader = true;
let currentPacketType = 0;

function processIncomingData(data) {
  packetBuffer = Buffer.concat([packetBuffer, data]);
  
  while (packetBuffer.length > 0) {
    if (parsingHeader) {
      if (packetBuffer.length < 4) {
        // Not enough data for header yet
        return;
      }
      
      // Check for sync byte
      if (packetBuffer[0] !== 0xAA) {
        // Invalid sync byte, try to find a valid one
        const nextSyncIndex = packetBuffer.indexOf(0xAA, 1);
        if (nextSyncIndex === -1) {
          // No sync byte found, discard all data
          packetBuffer = Buffer.alloc(0);
          return;
        }
        // Found a sync byte, discard data up to it
        packetBuffer = packetBuffer.slice(nextSyncIndex);
        continue;
      }
      
      currentPacketType = packetBuffer[1];
      expectedPacketLength = packetBuffer[2] + 6; // Header (4) + Length + CRC (2)
      parsingHeader = false;
    }
    
    if (packetBuffer.length < expectedPacketLength) {
      // Not enough data for complete packet yet
      return;
    }
    
    // We have a complete packet
    const packet = packetBuffer.slice(0, expectedPacketLength);
    packetBuffer = packetBuffer.slice(expectedPacketLength);
    parsingHeader = true;
    
    // Parse the packet based on its type
    parsePacket(packet);
  }
}

function parsePacket(packet) {
  const type = packet[1];
  const length = packet[2];
  const sequence = packet[3];
  const data = packet.slice(4, 4 + length);
  
  let parsedData = null;
  
  switch (type) {
    case 0x80: // USB_PACKET
      parsedData = parseUsbPacket(data);
      break;
    case 0x81: // STATE_CHANGE
      parsedData = parseStateChange(data);
      break;
    case 0x82: // STATUS_REPORT
      parsedData = parseStatusReport(data);
      break;
    case 0x83: // ERROR_REPORT
      parsedData = parseErrorReport(data);
      break;
    default:
      parsedData = { rawData: Array.from(data) };
  }
  
  const packetInfo = {
    type: PACKET_TYPES[type] || `UNKNOWN(0x${type.toString(16)})`,
    sequence,
    data: parsedData
  };
  
  mainWindow.webContents.send('packet:received', packetInfo);
}

function parseUsbPacket(data) {
  if (data.length < 8) {
    return { error: 'Invalid USB packet size' };
  }
  
  const timestamp = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
  const pid = data[4];
  const devAddr = data[5];
  const endpoint = data[6];
  const crcValid = !!(data[7] & 0x80);
  const packetData = data.length > 8 ? data.slice(8) : null;
  
  return {
    timestamp,
    pid,
    devAddr,
    endpoint,
    crcValid,
    data: packetData ? Array.from(packetData) : []
  };
}

function parseStateChange(data) {
  if (data.length < 1) {
    return { error: 'Invalid state change packet' };
  }
  
  const states = ['DISCONNECTED', 'CONNECTED', 'RESET'];
  const state = data[0] <= 2 ? states[data[0]] : `UNKNOWN(${data[0]})`;
  
  let speed = null;
  if (data.length > 1 && data[0] === 1) {
    speed = data[1] === 0 ? 'LOW_SPEED' : 'FULL_SPEED';
  }
  
  return { state, speed };
}

function parseStatusReport(data) {
  if (data.length < 4) {
    return { error: 'Invalid status report packet' };
  }
  
  const deviceCount = data[0];
  const captureState = data[1] ? 'ACTIVE' : 'IDLE';
  const bufferUsage = (data[2] << 8) | data[3];
  
  return { deviceCount, captureState, bufferUsage };
}

function parseErrorReport(data) {
  if (data.length < 2) {
    return { error: 'Invalid error report packet' };
  }
  
  const errorCodes = [
    'NONE',
    'INVALID_COMMAND',
    'BUFFER_OVERFLOW',
    'CRC_FAILURE',
    'INVALID_STATE',
    'USB_ERROR',
    'TIMEOUT',
    'INTERNAL'
  ];
  
  const errorCode = data[0] < errorCodes.length ? errorCodes[data[0]] : `UNKNOWN(${data[0]})`;
  const context = data[1];
  
  return { errorCode, context };
}

// IPC Event Handlers
ipcMain.handle('serial:scan-ports', async () => {
  return await scanPorts();
});

ipcMain.on('serial:connect', (event, { port, baudRate }) => {
  connectToDevice(port, baudRate);
});

ipcMain.on('serial:disconnect', () => {
  disconnectFromDevice();
});

ipcMain.on('capture:start', (event, config) => {
  startCapture(config);
});

ipcMain.on('capture:stop', () => {
  stopCapture();
});

app.on('ready', createWindow);

app.on('window-all-closed', () => {
  if (process.platform !== 'darwin') {
    app.quit();
  }
});

app.on('activate', () => {
  if (BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});

app.on('before-quit', () => {
  disconnectFromDevice();
}); 
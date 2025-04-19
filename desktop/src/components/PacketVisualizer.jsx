import React, { useState, useEffect, useRef } from 'react';
import * as usbDecoder from '../utils/usb-decoder';
import TransactionAnalyzer from '../utils/transaction-analyzer';
import './PacketVisualizer.css';

/**
 * Component for visualizing USB packets and transactions
 */
const PacketVisualizer = ({ packets, selectedPacketId, onPacketSelect }) => {
  const [transactions, setTransactions] = useState([]);
  const [filter, setFilter] = useState({
    pid: '',
    deviceAddress: '',
    endpoint: '',
    showTokens: true,
    showData: true,
    showHandshake: true,
    showSof: false
  });
  const [view, setView] = useState('transactions'); // 'transactions' or 'packets'
  const analyzer = useRef(new TransactionAnalyzer());
  
  // Process packets into transactions whenever packets change
  useEffect(() => {
    if (!packets || packets.length === 0) {
      setTransactions([]);
      return;
    }
    
    analyzer.current.reset();
    analyzer.current.processPackets(packets);
    setTransactions(analyzer.current.getAllTransactions());
  }, [packets]);
  
  // Apply filters to packets
  const filteredPackets = React.useMemo(() => {
    if (!packets) return [];
    
    return packets.filter(packet => {
      // Filter by PID type
      if (packet.pid) {
        const packetType = usbDecoder.getPacketType(packet.pid);
        if (packetType === 'Token' && !filter.showTokens) return false;
        if (packetType === 'Data' && !filter.showData) return false;
        if (packetType === 'Handshake' && !filter.showHandshake) return false;
        if (packet.pid === usbDecoder.PID.SOF && !filter.showSof) return false;
      }
      
      // Filter by specific PID
      if (filter.pid && packet.pid !== parseInt(filter.pid, 16)) return false;
      
      // Filter by device address and endpoint
      if (packet.deviceAddress !== undefined) {
        if (filter.deviceAddress && packet.deviceAddress !== parseInt(filter.deviceAddress)) return false;
        if (filter.endpoint && packet.endpoint !== parseInt(filter.endpoint)) return false;
      }
      
      return true;
    });
  }, [packets, filter]);
  
  // Apply filters to transactions
  const filteredTransactions = React.useMemo(() => {
    if (!transactions) return [];
    
    return transactions.filter(transaction => {
      // Filter by transaction type
      if (transaction.type === 'SOF' && !filter.showSof) return false;
      
      // Filter by device address and endpoint
      if (filter.deviceAddress && transaction.deviceAddress !== parseInt(filter.deviceAddress)) return false;
      if (filter.endpoint && transaction.endpoint !== parseInt(filter.endpoint)) return false;
      
      // Filter by specific PID (if any packet in the transaction matches)
      if (filter.pid) {
        const pidValue = parseInt(filter.pid, 16);
        const hasMatchingPid = transaction.packets.some(p => p.pid === pidValue);
        if (!hasMatchingPid) return false;
      }
      
      return true;
    });
  }, [transactions, filter]);
  
  // Handle filter changes
  const handleFilterChange = (e) => {
    const { name, value, type, checked } = e.target;
    setFilter(prev => ({
      ...prev,
      [name]: type === 'checkbox' ? checked : value
    }));
  };
  
  // Toggle view between packets and transactions
  const toggleView = () => {
    setView(prev => prev === 'transactions' ? 'packets' : 'transactions');
  };
  
  // Get background color for different packet types
  const getPacketColor = (packet) => {
    if (!packet.pid) return 'transparent';
    
    const packetType = usbDecoder.getPacketType(packet.pid);
    
    switch (packetType) {
      case 'Token':
        return 'rgba(173, 216, 230, 0.3)'; // Light blue
      case 'Data':
        return 'rgba(144, 238, 144, 0.3)'; // Light green
      case 'Handshake':
        return 'rgba(255, 182, 193, 0.3)'; // Light pink
      case 'Special':
        return 'rgba(255, 255, 224, 0.3)'; // Light yellow
      default:
        return 'transparent';
    }
  };
  
  // Render a packet row
  const renderPacket = (packet, isSelected) => {
    const packetType = packet.pid ? usbDecoder.getPacketType(packet.pid) : 'Unknown';
    const pidName = packet.pid ? usbDecoder.getPidName(packet.pid) : 'Unknown';
    
    let deviceInfo = '';
    if (packet.deviceAddress !== undefined) {
      deviceInfo = `Dev: ${packet.deviceAddress}, EP: ${packet.endpoint}`;
    } else if (packet.pid === usbDecoder.PID.SOF && packet.frameNumber !== undefined) {
      deviceInfo = `Frame: ${packet.frameNumber}`;
    }
    
    return (
      <div 
        key={packet.id}
        className={`packet-row ${isSelected ? 'selected' : ''}`}
        style={{ backgroundColor: getPacketColor(packet) }}
        onClick={() => onPacketSelect(packet.id)}
      >
        <div className="packet-time">{formatTimestamp(packet.timestamp)}</div>
        <div className="packet-type">{packetType}</div>
        <div className="packet-pid">{pidName}</div>
        <div className="packet-info">{deviceInfo}</div>
        <div className="packet-data">
          {packet.data && packet.data.length > 0 ? 
            `${packet.data.length} bytes` : ''}
        </div>
      </div>
    );
  };
  
  // Render a transaction row
  const renderTransaction = (transaction, isSelected) => {
    const firstPacket = transaction.packets[0] || {};
    const isExpanded = selectedPacketId && transaction.packets.some(p => p.id === selectedPacketId);
    
    return (
      <div key={transaction.id} className={`transaction ${isExpanded ? 'expanded' : ''}`}>
        <div 
          className={`transaction-header ${isSelected ? 'selected' : ''}`}
          onClick={() => onPacketSelect(firstPacket.id)}
        >
          <div className="transaction-time">{formatTimestamp(transaction.timestamp)}</div>
          <div className="transaction-type">{transaction.type}</div>
          <div className="transaction-info">
            {transaction.deviceAddress !== undefined ? 
              `Dev: ${transaction.deviceAddress}, EP: ${transaction.endpoint}` : ''}
          </div>
          <div className="transaction-status">{transaction.status}</div>
          <div className="transaction-description">{transaction.description}</div>
        </div>
        
        {isExpanded && (
          <div className="transaction-packets">
            {transaction.packets.map(packet => 
              renderPacket(packet, packet.id === selectedPacketId)
            )}
          </div>
        )}
      </div>
    );
  };
  
  // Format timestamp for display
  const formatTimestamp = (timestamp) => {
    if (!timestamp) return '';
    const date = new Date(timestamp);
    return `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}:${date.getSeconds().toString().padStart(2, '0')}.${date.getMilliseconds().toString().padStart(3, '0')}`;
  };
  
  // Render packet details panel
  const renderPacketDetails = () => {
    if (!selectedPacketId || !packets) return null;
    
    const packet = packets.find(p => p.id === selectedPacketId);
    if (!packet) return null;
    
    const pidName = packet.pid ? usbDecoder.getPidName(packet.pid) : 'Unknown';
    const packetType = packet.pid ? usbDecoder.getPacketType(packet.pid) : 'Unknown';
    
    let detailsContent = null;
    
    // Create details based on packet type
    if (packet.pid === usbDecoder.PID.SETUP && packet.data && packet.data.length >= 8) {
      const setup = usbDecoder.decodeSetupPacket(packet.data);
      detailsContent = (
        <div className="details-setup">
          <h4>Setup Packet</h4>
          <div><span>Request:</span> {setup.requestName}</div>
          <div><span>Type:</span> {setup.type}</div>
          <div><span>Direction:</span> {setup.direction}</div>
          <div><span>Recipient:</span> {setup.recipient}</div>
          <div><span>Value:</span> 0x{setup.wValue.toString(16).padStart(4, '0')}</div>
          <div><span>Index:</span> 0x{setup.wIndex.toString(16).padStart(4, '0')}</div>
          <div><span>Length:</span> {setup.wLength}</div>
          {setup.requestDetails && <div><span>Details:</span> {setup.requestDetails}</div>}
        </div>
      );
    } else if ((packet.pid === usbDecoder.PID.DATA0 || 
                packet.pid === usbDecoder.PID.DATA1 || 
                packet.pid === usbDecoder.PID.DATA2 || 
                packet.pid === usbDecoder.PID.MDATA) && 
                packet.data && packet.data.length > 0) {
      detailsContent = (
        <div className="details-data">
          <h4>Data Packet</h4>
          <div><span>Length:</span> {packet.data.length} bytes</div>
          <pre className="hex-dump">{usbDecoder.formatHexDump(packet.data)}</pre>
        </div>
      );
    } else if (packet.pid === usbDecoder.PID.SOF && packet.frameNumber !== undefined) {
      detailsContent = (
        <div className="details-sof">
          <h4>Start of Frame</h4>
          <div><span>Frame Number:</span> {packet.frameNumber}</div>
        </div>
      );
    } else if (packetType === 'Token') {
      detailsContent = (
        <div className="details-token">
          <h4>Token Packet</h4>
          <div><span>Device Address:</span> {packet.deviceAddress}</div>
          <div><span>Endpoint:</span> {packet.endpoint}</div>
        </div>
      );
    } else if (packetType === 'Handshake') {
      detailsContent = (
        <div className="details-handshake">
          <h4>Handshake Packet</h4>
          <div>
            {pidName === 'ACK' && 'Positive acknowledgement'}
            {pidName === 'NAK' && 'Negative acknowledgement - device not ready'}
            {pidName === 'STALL' && 'Endpoint halted or request not supported'}
            {pidName === 'NYET' && 'Not yet - no data available'}
          </div>
        </div>
      );
    }
    
    return (
      <div className="packet-details">
        <div className="details-header">
          <h3>Packet Details</h3>
          <div><span>PID:</span> {pidName} (0x{packet.pid?.toString(16).padStart(2, '0').toUpperCase()})</div>
          <div><span>Type:</span> {packetType}</div>
          <div><span>Time:</span> {formatTimestamp(packet.timestamp)}</div>
        </div>
        
        {detailsContent}
      </div>
    );
  };
  
  return (
    <div className="packet-visualizer">
      <div className="visualizer-toolbar">
        <div className="filter-controls">
          <div className="filter-group">
            <label>
              <input 
                type="checkbox" 
                name="showTokens" 
                checked={filter.showTokens} 
                onChange={handleFilterChange}
              />
              Token Packets
            </label>
            <label>
              <input 
                type="checkbox" 
                name="showData" 
                checked={filter.showData} 
                onChange={handleFilterChange}
              />
              Data Packets
            </label>
            <label>
              <input 
                type="checkbox" 
                name="showHandshake" 
                checked={filter.showHandshake} 
                onChange={handleFilterChange}
              />
              Handshake Packets
            </label>
            <label>
              <input 
                type="checkbox" 
                name="showSof" 
                checked={filter.showSof} 
                onChange={handleFilterChange}
              />
              SOF Packets
            </label>
          </div>
          
          <div className="filter-group">
            <input 
              type="text" 
              name="deviceAddress" 
              value={filter.deviceAddress} 
              onChange={handleFilterChange} 
              placeholder="Device ID"
              size="10"
            />
            <input 
              type="text" 
              name="endpoint" 
              value={filter.endpoint} 
              onChange={handleFilterChange} 
              placeholder="Endpoint"
              size="8"
            />
            <input 
              type="text" 
              name="pid" 
              value={filter.pid} 
              onChange={handleFilterChange} 
              placeholder="PID (hex)"
              size="8"
            />
          </div>
          
          <button onClick={toggleView} className="view-toggle-btn">
            {view === 'transactions' ? 'Show Raw Packets' : 'Show Transactions'}
          </button>
        </div>
      </div>
      
      <div className="visualizer-content">
        <div className="packet-list">
          {view === 'transactions' ? (
            filteredTransactions.map(transaction => renderTransaction(
              transaction, 
              selectedPacketId && transaction.packets.some(p => p.id === selectedPacketId)
            ))
          ) : (
            filteredPackets.map(packet => renderPacket(packet, packet.id === selectedPacketId))
          )}
        </div>
        
        {renderPacketDetails()}
      </div>
    </div>
  );
};

export default PacketVisualizer; 
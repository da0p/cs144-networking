#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

#define FIVE_THOUSANDS_MS     5000
#define THIRTY_THOUSANDS_MS   30000

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();
    
    EthernetFrame eth_fr;

    std::map<uint32_t, EthernetAddress>::iterator ip_it;
    std::map<uint32_t, std::queue<InternetDatagram>>::iterator dg_it;
    std::map<uint32_t, uint64_t>::iterator to_it;

    dg_it = _dg_map.find(next_hop_ip);
    //! Make sure initialization of _dg_map
    if (dg_it == _dg_map.end()){
        std::queue<InternetDatagram> q;
        _dg_map.insert(std::pair<uint32_t, std::queue<InternetDatagram>>(next_hop_ip, q));
    }
 
    to_it = _ip_timeout_table.find(next_hop_ip);   

    ip_it = _ip_to_eth_table.find(next_hop_ip);
        
    if (ip_it != _ip_to_eth_table.end()) {
        
       //! Send out the ethernet frame
        _dg_map[next_hop_ip].push(dgram);
        flush_buffer(next_hop_ip, ip_it->second);
    }
    else {

        ARPMessage arp_req;

        //! Queue the datagram
        _dg_map[next_hop_ip].push(dgram);

        //! Check the timeout table
        if (to_it == _ip_timeout_table.end() || _system_time - to_it->second > FIVE_THOUSANDS_MS) { 
            
            //! Set ARP type
            eth_fr.header().type = EthernetHeader::TYPE_ARP;
            //! Set source ethernet address
            eth_fr.header().src = _ethernet_address;
            //! Set ethernet broadcast address
            eth_fr.header().dst = ETHERNET_BROADCAST;
            //! Set ARP Request Code
            arp_req.opcode = ARPMessage::OPCODE_REQUEST;
            //! Set Sender Ethernet Address
            arp_req.sender_ethernet_address = _ethernet_address;
            //! Set sender ip address
            arp_req.sender_ip_address = _ip_address.ipv4_numeric();
            //! set target ip address that the host wants to request
            arp_req.target_ip_address = next_hop_ip;
            //! Serialize the ARP request
            eth_fr.payload() = arp_req.serialize(); 
            //! send out the arp
            frames_out().push(eth_fr);
            //! Add the arp into the timeout map
            if (to_it == _ip_timeout_table.end()) {
                _ip_timeout_table.insert(std::pair<uint32_t, uint64_t>(next_hop_ip, _system_time));
            }

        }
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {

    EthernetFrame eth_fr;
    Buffer payload{frame.payload().concatenate()};

    if (frame.header().dst != ETHERNET_BROADCAST && frame.header().dst != _ethernet_address)
        return {};

    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        
       InternetDatagram dgram;

       if (dgram.parse(payload) == ParseResult::NoError) {
            
            return dgram;
       }
    }
    else if (frame.header().type == EthernetHeader::TYPE_ARP) {
        ARPMessage arp_req;

        if (arp_req.parse(payload) == ParseResult::NoError) { 

            //! Update table
            std::map<uint32_t, EthernetAddress>::iterator it;

            it = _ip_to_eth_table.find(arp_req.sender_ip_address);

            if (it == _ip_to_eth_table.end()) {

                _ip_to_eth_table.insert(std::pair<uint32_t, EthernetAddress>(
                                arp_req.sender_ip_address, frame.header().src));
                    _ip_timeout_table.insert(std::pair<uint32_t, uint64_t>(arp_req.sender_ip_address, _system_time));
                }
            if (arp_req.target_ip_address == _ip_address.ipv4_numeric() && arp_req.opcode == ARPMessage::OPCODE_REQUEST) {
                //! Send a ARP reply
                ARPMessage arp_reply;

                //! Set ARP type
                eth_fr.header().type = EthernetHeader::TYPE_ARP;
                //! Set source ethernet address
                eth_fr.header().src = _ethernet_address;
                //! Set destination ethernet address
                eth_fr.header().dst = frame.header().src;
                //! Set ARP reply opcode
                arp_reply.opcode = ARPMessage::OPCODE_REPLY;
                //! Set Sender Ethernet Address
                arp_reply.sender_ethernet_address = _ethernet_address;
                //! Set Target Ethernet Address
                arp_reply.target_ethernet_address = frame.header().src;
                //! Set source arp address
                arp_reply.sender_ip_address = _ip_address.ipv4_numeric();
                //! Set target arp address
                arp_reply.target_ip_address = arp_req.sender_ip_address;
                //! Serialize arp reply
                eth_fr.payload() = arp_reply.serialize();
                //! Send out the frame
                frames_out().push(eth_fr);
            }
            else if (arp_req.target_ip_address == _ip_address.ipv4_numeric() && arp_req.opcode == ARPMessage::OPCODE_REPLY) {
                flush_buffer(arp_req.sender_ip_address, arp_req.sender_ethernet_address);
            }
        }
    }

    return {};
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {

   _system_time += ms_since_last_tick;

    std::map<uint32_t, uint64_t>::iterator to_it;
    std::queue<uint32_t> q;

    for (to_it = _ip_timeout_table.begin(); to_it != _ip_timeout_table.end(); ++to_it) {
        if (_system_time - to_it->second > THIRTY_THOUSANDS_MS) {
            _ip_to_eth_table.erase(to_it->first);
            q.push(to_it->first);
        }
    }

    while (!q.empty()) {
        _ip_timeout_table.erase(q.front());
        q.pop();
    }
}

void NetworkInterface::flush_buffer(uint32_t ip_addr, EthernetAddress eth_addr) {
    
    EthernetFrame eth_fr;

    while (!_dg_map[ip_addr].empty()) {
    	auto dg = _dg_map[ip_addr].front();
        //! Set IPv4 header type
        eth_fr.header().type = EthernetHeader::TYPE_IPv4;
        //! Set source ethernet address
        eth_fr.header().src = _ethernet_address;
        //! Set destination ethernet address
        eth_fr.header().dst = eth_addr;
        //! Serialize the datagram
        eth_fr.payload() = dg.serialize();

        _dg_map[ip_addr].pop();

        frames_out().push(eth_fr);
    }
}

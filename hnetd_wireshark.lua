-- -*-lua-*-
--
-- $Id: hnetd_wireshark.lua $
--
-- Author: Markus Stenberg <mstenber@cisco.com>
--
-- Copyright (c) 2013 cisco Systems, Inc.
--
-- Created:       Tue Dec  3 11:13:05 2013 mstenber
-- Last modified: Thu Feb 13 11:44:59 2014 mstenber
-- Edit time:     103 min
--

-- This is Lua module which provides VERY basic dissector for TLVs we
-- transmit.

-- Usage: wireshark -X lua_script:hnetd_wireshark.lua

p_hncp = Proto("hncp", "Homenet Control Protocol")

local f_id = ProtoField.uint16('hncp.id', 'TLV id')
local f_len = ProtoField.uint16('hncp.len', 'TLV len')
local f_data = ProtoField.bytes('hncp.data', 'TLV data', base.HEX)

local f_nid_hash = ProtoField.bytes('hncp.node_identifier_hash', 
                                    'Node identifier', base.HEX)
local f_data_hash = ProtoField.bytes('hncp.data_hash',
                                     'Node data hash', base.HEX)
local f_network_hash = ProtoField.bytes('hncp.network_hash',
                                        'Network state hash', base.HEX)

local f_lid = ProtoField.uint32('hncp.llid', 'Local link identifier')
local f_rlid = ProtoField.uint32('hncp.rlid', 'Remote link identifier')
local f_upd = ProtoField.uint32('hncp.update_number', 'Update number')
local f_ms = ProtoField.uint32('hncp.ms_since_origination', 
                               'Time since origination (ms)')

p_hncp.fields = {f_id, f_len, f_data,
                 f_nid_hash, f_data_hash, f_network_hash,
                 f_lid, f_rlid, f_upd, f_ms}

local tlvs = {
   [1]={name='link-id', 
        contents={{16, f_nid_hash},
                  {4, f_lid}},
   },
   [2]={name='req-net-hash'},
   [3]={name='req-node-data',
        contents={{16, f_nid_hash}},
   },

   [4]={name='network-hash',
        contents={{16, f_network_hash}}},

   [5]={name='node-state',
        contents={{16, f_nid_hash},
                  {4, f_upd},
                  {4, f_ms},
                  {16, f_data_hash},
        }
   },
   [6]={name='node-data', 
        contents={{16, f_nid_hash},
                  {4, f_upd}},
        recurse=true},
   [7]={name='node-data-key'},
   [8]={name='node-data-neighbor', contents={{16, f_nid_hash},
                                             {4, f_rlid},
                                             {4, f_lid},
                                            },
   },
   [9]={name='custom'},
   [41]={name='external-connection', contents={}, recurse=true},
   [42]={name='delegated-prefix'},
   [43]={name='assigned-prefix'},
   [44]={name='dhcp-options'},
   [45]={name='dhcpv6-options'},
   [46]={name='router-address'},

   [50]={name='dns-delegated-zone'},
   [51]={name='dns-router-name'},
   [52]={name='dns-domain-name'},
   [60]={name='routing-protocol'},
}


function p_hncp.dissector(buffer, pinfo, tree)
   pinfo.cols.protocol = 'hncp'

   local rec_decode
   local function data_decode(ofs, left, tlv, tree)
      for i, v in ipairs(tlv.contents)
      do
         local elen, ef = unpack(v)
         if elen > left
         then
            tree:append_text(string.format(' (!!! missing data - %d > %d (%s))',
                                           elen, left, v))
            return
         end
         tree:add(ef, buffer(ofs, elen))
         left = left - elen
         ofs = ofs + elen
      end
      if tlv.recurse 
      then
         rec_decode(ofs, left, tree)
      end
   end

   rec_decode = function (ofs, left, tree)
      if left < 4
      then
         return
      end
      local partial
      local rid = buffer(ofs, 2)
      local rlen = buffer(ofs+2, 2)
      local id = rid:uint()
      local len = rlen:uint() 
      local bs = ''
      local ps = ''
      local tlv = tlvs[id] or {}
      if tlv.name
      then
         bs = ' (' .. tlv.name .. ')'
      end
      if len < 4
      then
         ps = ' (broken - len<4)'
         partial = true
      end
      if len > left
      then
         len = left
         ps = ' (partial)'
         partial = true
      end
      local tree2 = tree:add(buffer(ofs, len), 
                             string.format('TLV %d%s - %d data bytes%s',
                                           id, bs, len, ps))
      if partial
      then
         return
      end
      local fid = tree2:add(f_id, rid)
      fid:append_text(bs)
      local flen = tree2:add(f_len, rlen)
      if len > 4 
      then
         local fdata = tree2:add(f_data, buffer(ofs + 4, len - 4))
         if tlv.contents
         then
            -- skip the tlv header (that's why +- 4)
            data_decode(ofs + 4, len - 4, tlv, fdata)
         end
      end
      -- recursively decode the rest too, hrr :)

      -- (note that we have to round it up to next /4 boundary; stupid
      -- alignment..)
      len = math.floor((len + 3)/4) * 4

      rec_decode(ofs + len, left - len, tree)
   end
   rec_decode(0, buffer:len(), tree:add(p_hncp, buffer()))
end

-- register as udp dissector
udp_table = DissectorTable.get("udp.port")
udp_table:add(8808, p_hncp)
udp_table:add(38808, p_hncp)

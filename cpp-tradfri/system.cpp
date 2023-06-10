//
//  system.cpp
//  cpp-tradfri
//
//  Created by Piotr Brzeski on 2023-01-06.
//  Copyright © 2023 Brzeski.net. All rights reserved.
//

#include "system.h"
#include "json.h"
#include "bulb.h"
#include "exception.h"
#include <vector>
#include <variant>
//#include <algorithm>
//#include <iostream>

using namespace tradfri;

namespace {

std::vector<std::string> parse_ids(std::string const& list) {
	std::vector<std::string> ids;
	auto begin = list.find('[');
	while(begin != std::string::npos) {
		++begin;
		auto end = list.find(',', begin);
		if(end != std::string::npos) {
			if(ids.empty()) {
				auto expected_number_of_ids = list.size()/end;
				ids.reserve(expected_number_of_ids);
			}
			ids.emplace_back(list.data() + begin, end - begin);
		}
		else {
			auto end = list.find(']', begin);
			if(end != std::string::npos) {
				ids.emplace_back(list.data() + begin, end - begin);
			}
		}
		begin = end;
	}
	return ids;
}

template<typename Devices, typename... Args>
std::variant<bulb*, plug*> get_device(std::string const& device_name, Devices& devices, Args&... more_devices) {
	for(auto& device : devices) {
		if(device.name() == device_name) {
			return &device;
		}
	}
	if constexpr(sizeof...(more_devices) > 0) {
		return get_device(device_name, more_devices...);
	}
	else {
		throw exception("Device \"" + device_name + "\" not found.");
	}
}

}

system::system(configuration const& configuration)
	: m_coap(configuration.ip, configuration.identity, configuration.key)
{
}

system::system(std::string const& ip, std::string const& identity, std::string const& key)
	: m_coap(ip, identity, key)
{
}

void system::enumerate_devices() {
	static auto const devices_uri = std::string("15001");
	static auto const groups_uri = std::string("15004");
	auto list = m_coap.get(devices_uri);
	auto ids = parse_ids(list);
	for(auto& id : ids) {
		load_device(id);
	}
//	std::sort(m_bulbs.begin(), m_bulbs.end(), [](auto& b1, auto& b2){ return b1.name() < b2.name(); });
	
	list = m_coap.get(groups_uri);
	ids = parse_ids(list);
//	for(auto& id : ids) {
//		load_device(id);
//	}
}

std::function<void()> system::toggle_operation(std::string const& device_name) {
	auto device = get_device(device_name, m_bulbs, m_plugs);
	return [device](){ std::visit([](auto&& device){ device->toggle(); }, device); };
}

void system::load_device(std::string const& id) {
	static auto const type_key = std::string("5750");
	auto device_description = device::load(m_coap, id);
	auto device_json = json(std::move(device_description));
	auto type = device_json[type_key].get_int();
	switch(type) {
		case bulb::system_id:
			m_bulbs.push_back(bulb::load(id, m_coap, device_json));
			break;
		case plug::system_id:
			m_plugs.push_back(plug::load(id, m_coap, device_json));
			break;
		default:
//			std::cout << id << ", type=" << type << ", " << device_json["9001"].get_string() << std::endl;
			break;
	}
}

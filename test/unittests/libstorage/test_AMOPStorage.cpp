/*
 * test_AMOPStorage.cpp
 *
 *  Created on: 2019-3-25
 *      Author: ancelmo
 */

#include "libstorage/AMOPStorage.h"
#include <boost/test/unit_test.hpp>
#include <libstorage/AMOPStorage.h>
#include <libstorage/Table.h>
#include <libdevcore/FixedHash.h>
#include <libchannelserver/ChannelRPCServer.h>
#include <libstorage/StorageException.h>

using namespace dev;
using namespace dev::storage;

namespace test_AMOPStateStorage
{

class MockChannelRPCServer: public dev::ChannelRPCServer {
public:
	virtual dev::channel::TopicChannelMessage::Ptr pushChannelMessage(
	        dev::channel::TopicChannelMessage::Ptr message) override {
		std::string jsonStr(message->data(), message->data() + message->dataSize());

		std::stringstream ssIn;
		ssIn << jsonStr;

		Json::Value requestJson;
		ssIn >> requestJson;

		Json::Value responseJson;



		if(requestJson["op"].asString() == "select") {
			if(requestJson["params"]["table"].asString() == "e") {
				BOOST_THROW_EXCEPTION(StorageException(-1, "mock exception"));
			}

			if(requestJson["params"]["key"].asString() != "LiSi") {
				responseJson["code"] = 0;
			}
			else if(requestJson["params"]["condition"].isArray() && requestJson["params"]["condition"][0][2].asString() == "2") {
				responseJson["code"] = 0;
			}
			else {
				Json::Value resultJson;
				resultJson["columns"].append("Name");
				resultJson["columns"].append("id");

				Json::Value entry;
				entry.append("LiSi");
				entry.append("1");

				resultJson["data"].append(entry);

				responseJson["code"] = 0;
				responseJson["result"] = resultJson;
			}
		}
		else if(requestJson["op"].asString() == "commit") {
			size_t count = 0;
			for(auto it: requestJson["params"]["data"]) {
				++count;
				if(it["table"] == "e") {
					BOOST_THROW_EXCEPTION(StorageException(-1, "mock exception"));
				}
			}

			responseJson["result"]["count"] = count;
			responseJson["code"] = 0;
		}

		std::string responseStr = responseJson.toStyledString();
		LOG(TRACE) << "AMOP Storage response:" << responseStr;

		auto response = std::make_shared<dev::channel::TopicChannelMessage>();
		response->setResult(0);
		response->setSeq(message->seq());
		response->setTopic(message->topic());
		response->setType(0x31);
		response->setData((const unsigned char*)responseStr.data(), responseStr.size());

		return response;
	}
};

struct AMOPFixture
{
    AMOPFixture()
    {
        AMOP = std::make_shared<dev::storage::AMOPStorage>();
        std::shared_ptr<MockChannelRPCServer> mockChannel = std::make_shared<MockChannelRPCServer>();
        AMOP->setChannelRPCServer(mockChannel);
    }
    Entries::Ptr getEntries()
    {
        Entries::Ptr entries = std::make_shared<Entries>();
        Entry::Ptr entry = std::make_shared<Entry>();
        entry->setField("Name", "LiSi");
        entry->setField("id", "1");
        entries->addEntry(entry);
        return entries;
    }
    dev::storage::AMOPStorage::Ptr AMOP;
};

BOOST_FIXTURE_TEST_SUITE(AMOP, AMOPFixture)


BOOST_AUTO_TEST_CASE(onlyDirty)
{
    BOOST_CHECK_EQUAL(AMOP->onlyDirty(), true);
}

BOOST_AUTO_TEST_CASE(empty_select)
{
    h256 h(0x01);
    int num = 1;
    std::string table("t_test");
    std::string key("id");
    Entries::Ptr entries = AMOP->select(h, num, table, key, std::make_shared<Condition>());
    BOOST_CHECK_EQUAL(entries->size(), 0u);
}

BOOST_AUTO_TEST_CASE(select_condition) {
	h256 h(0x01);
	int num = 1;
	std::string table("t_test");
	auto condition = std::make_shared<Condition>();
	condition->EQ("id", "2");
	Entries::Ptr entries = AMOP->select(h, num, table, "LiSi", condition);
	BOOST_CHECK_EQUAL(entries->size(), 0u);

	condition = std::make_shared<Condition>();
	condition->EQ("id", "1");
	entries = AMOP->select(h, num, table, "LiSi", condition);
	BOOST_CHECK_EQUAL(entries->size(), 1u);
}

BOOST_AUTO_TEST_CASE(commit)
{
    h256 h(0x01);
    int num = 1;
    h256 blockHash(0x11231);
    std::vector<dev::storage::TableData::Ptr> datas;
    dev::storage::TableData::Ptr tableData = std::make_shared<dev::storage::TableData>();
    tableData->info->name = "t_test";
    tableData->info->key = "Name";
    tableData->info->fields.push_back("id");
    Entries::Ptr entries = getEntries();
    tableData->entries = entries;
    datas.push_back(tableData);
    size_t c = AMOP->commit(h, num, datas, blockHash);
    BOOST_CHECK_EQUAL(c, 1u);
    std::string table("t_test");
    std::string key("LiSi");
    entries = AMOP->select(h, num, table, key, std::make_shared<Condition>());
    BOOST_CHECK_EQUAL(entries->size(), 1u);
}

BOOST_AUTO_TEST_CASE(exception)
{
#if 0
    h256 h(0x01);
    int num = 1;
    h256 blockHash(0x11231);
    std::vector<dev::storage::TableData::Ptr> datas;
    dev::storage::TableData::Ptr tableData = std::make_shared<dev::storage::TableData>();
    tableData->info->name = "e";
    tableData->info->key = "Name";
    tableData->info->fields.push_back("id");
    Entries::Ptr entries = getEntries();
    entries->get(0)->setField("Name", "Exception");
    tableData->entries = entries;
    datas.push_back(tableData);
    BOOST_CHECK_THROW(AMOP->commit(h, num, datas, blockHash), boost::exception);
    std::string table("e");
    std::string key("Exception");

    BOOST_CHECK_THROW(AMOP->select(h, num, table, key, std::make_shared<Condition>()), boost::exception);
#endif
}

BOOST_AUTO_TEST_SUITE_END()

}  // namespace test_AMOPStateStorage

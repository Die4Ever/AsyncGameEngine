#include "pch.h"
#include "CppUnitTest.h"
#include "../AsyncGameEngine/AsyncGame.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using std::string;

bool HandleMessage(Message& m, MessageBlock& mb) {
	return true;
}

bool Update(MessageBlock& mb) {
	return true;
}

namespace AsyncGameEngineTests
{

	TEST_CLASS(AsyncGameEngineTests)
	{
	public:

		TEST_METHOD(TestParseKeyValuesTest)
		{
			AssertMap(map);
			string smap2 = WriteKeyValues(map);
			Assert::IsTrue(smap2.length() > 0);
			auto map2 = ParseKeyValues(smap2);
			AssertMap(map2);
			auto map3 = ParseKeyValues(smap2 + "\n\n#comment\r\n\r\nbla=fff");
			Assert::IsTrue(map3.size() == 3);
			Assert::AreEqual(map3["bla"], string("fff"));

			auto m = map3;
			for (int i = 0; i < 1000; i++) {
				m[RandString(15)] = RandString(20);
			}
			string s = WriteKeyValues(m);
			auto m2 = ParseKeyValues(s);
			Assert::AreEqual(m.size(), m2.size());
		}

		TEST_METHOD(CheckTypes)
		{
			std::atomic<Uint32Pair> t2;
			Assert::IsTrue(t2.is_lock_free());
			Assert::AreEqual(sizeof(Uint32Pair), (size_t)8);
			Assert::AreEqual(sizeof(t2), sizeof(Uint32Pair));
		}

		TEST_METHOD(TestMessageQueue)
		{
			SharedMem sm;
			MessageQueue& mq = *CreateSharedMessageQueue(sm, "foobar");

			MessageBlock mb;
			InitGenericMessageBlock(mb);
			mq.SendMessageBlock(mb.header, mb.data);
			
			uint cursor = mq.GetStart();
			for (auto m : mq.EachMessage(cursor)) {
				string s = (char*)m.data;
				auto m = ParseKeyValues(s);
				AssertMap(m);
			}
			Assert::IsTrue(cursor > 0);
		}

		TEST_METHOD(TestMessageQueueIssue16)
		{
			//test regression for issue #16
			// https://github.com/Die4Ever/AsyncGameEngine/issues/16
			SharedMem sm;
			MessageQueue& mq = *CreateSharedMessageQueue(sm, "foobar");

			uint cursor = mq.GetStart();

			MessageBlock mb;
			MessageBlock mb2;
			InitGenericMessageBlock(mb);
			InitGenericMessageBlock(mb2);
			mb2.header.sender_id = GetCurrentProcessId();
			mq.SendMessageBlock(mb2.header, mb2.data);

			mq.Cleanup();//we are the only listener, so our sent message will get cleaned up

			mq.SendMessageBlock(mb.header, mb.data);//resend the message from sender_id 0, we want to read this	
			int issue_16_check = 0;
			for (auto m : mq.EachMessage(cursor)) {
				issue_16_check++;

				string s = (char*)m.data;
				auto m = ParseKeyValues(s);
				AssertMap(m);
			}
			Assert::AreEqual(issue_16_check, 1);
		}

		TEST_METHOD(TestQueueWrap)
		{
			SharedMem sm;
			MessageQueue& mq = *CreateSharedMessageQueue(sm, "foobar");
			uint cursor = mq.GetStart();
			MessageBlock mb;
			InitGenericMessageBlock(mb);

			for (int i = 0; i < MESSAGE_QUEUE_LEN; i++) {//yea we're doing way more than 1 loop of the queue
				mq.SendMessageBlock(mb.header, mb.data);//resend the message from sender_id 0, we want to read this
				int loop_test = 0;
				for (auto m : mq.EachMessage(cursor)) {
					loop_test++;
					string s = (char*)m.data;
					auto m = ParseKeyValues(s);
					AssertMap(m);
				}
				Assert::AreEqual(loop_test, 1);
				mq.Cleanup();
			}
		}

		string smap;
		std::unordered_map<string, string> map;

		AsyncGameEngineTests() {
			smap = "foo=bar\n#test comment\nkey=value";
			map["foo"] = "bar";
			map["key"] = "value";
		}

		void AssertMap(std::unordered_map<string, string>& m) {
			Assert::AreEqual(m.size(), (size_t)2);
			Assert::AreEqual(m["foo"], string("bar"));
			Assert::AreEqual(m["key"], string("value"));
		}

		void InitGenericMessageBlock(MessageBlock& mb)
		{
			mb.header.len = sizeof(mb.header);
			mb.header.read_count = 0;
			mb.header.sender_id = 0;//we can't receive a message that we sent
			auto m = mb.reserve();
			strcpy_s((char*)m.data, 1024, smap.c_str());
			m.SetDataLen((ushort)strlen((char*)m.data) + 1);
			mb.push(m);
		}

		string RandString(int len)
		{
			char buff[128];
			int min = 'a';
			int max = 'z';
			Assert::IsTrue(len < sizeof(buff));
			for (int i = 0; i < len; i++) {
				buff[i] = rand() % (max-min);
				buff[i] += min;
			}
			buff[len] = 0;
			return string(buff);
		}
	};
}

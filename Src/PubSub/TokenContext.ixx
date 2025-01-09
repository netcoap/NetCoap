module;

#include <iostream>
#include <functional>
#include <memory>

export module PubSub:TokenContext;

import Coap;
import Toolbox;

using namespace std;
using namespace netcoap::coap;
using namespace netcoap::toolbox;

namespace netcoap {
	namespace pubsub {

		export class TokenContext {
		public:

			static const string TOPIC_CONTROL;

			enum class STATUS { SUCCESS, FAILED, MAX_RETRY_EXCEED };

			enum class CALLBACK_TYPE { NONE, ONCE, RECURRENT };

			using ResponseCb = function<void(
				STATUS status, const shared_ptr<Message> respMsg)>;

			TokenContext(
				ResponseCb respCb, CALLBACK_TYPE cbType,
				shared_ptr<Message> msg) {

				m_cb = respCb;
				m_cbType = cbType;
				m_msg = msg;
			}

			inline shared_ptr<Message> getMsg() {
				return m_msg;
			}

			inline void setMsg(shared_ptr<Message> msg) {
				m_msg = msg;
			}

			inline Block1Xfer& getBlock1Xfer() {
				return m_block1Xfer;
			}

			inline Block2Xfer& getBlock2Xfer() {
				return m_block2Xfer;
			}

			inline CALLBACK_TYPE getCbType() {
				return m_cbType;
			}

			inline ResponseCb getCb() {
				return m_cb;
			}

			inline string getTopicData() {
				return m_topicData;
			}

			inline void setTopicData(string topicData) {
				m_topicData = topicData;
			}

		private:
			Block1Xfer m_block1Xfer;
			Block2Xfer m_block2Xfer;

			CALLBACK_TYPE m_cbType = CALLBACK_TYPE::NONE;
			ResponseCb m_cb;
			shared_ptr<Message> m_msg;

			string m_topicData = TOPIC_CONTROL;
		};

		const string TokenContext::TOPIC_CONTROL = "TOPIC_CONTROL_DATA";
	}
}
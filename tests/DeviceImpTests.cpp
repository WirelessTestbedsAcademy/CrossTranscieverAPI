#include "CppUTest/CommandLineTestRunner.h"

#include "DeviceImp.hpp"

void create_config_list(VESNA::ConfigList& cl);

class TestReceiver : public Transceiver::I_ReceiveDataPush
{
	public:
		int sample_count;
		boost::mutex sample_count_m;

		void pushBBSamplesRx(Transceiver::BBPacket* thePushedPacket,
				Transceiver::Boolean endOfBurst) {
			boost::unique_lock<boost::mutex> lock(sample_count_m);
			sample_count += thePushedPacket->SampleNumber;
		};
		TestReceiver() : sample_count(0) {};
		~TestReceiver() {};
};

class TestSpectrumSensor : public VESNA::I_SpectrumSensor
{
	public:
		VESNA::ConfigList* get_config_list()
		{
			VESNA::ConfigList* cl = new VESNA::ConfigList();
			create_config_list(*cl);
			return cl;
		}

		void sample_run(const VESNA::SweepConfig* sc, VESNA::sample_run_cb_t cb, void* priv)
		{
			VESNA::TimestampedData samples;
			samples.channel = sc->start_ch;

			int n;
			for(n = 0; n < sc->nsamples; n++) {
				samples.data.push_back(0);
			}

			while(1) {
				bool cont = cb(sc, &samples, priv);
				if(!cont) {
					break;
				}

				samples.timestamp += 1.;
			}
		}
};

TEST_GROUP(DeviceImpTestGroup)
{
};

TEST(DeviceImpTestGroup, TestConstructor)
{
	TestReceiver rx;
	TestSpectrumSensor ss;
	DeviceImp di(&rx, &ss);
}

TEST(DeviceImpTestGroup, TestCreateRXProfile)
{
	TestReceiver rx;
	TestSpectrumSensor ss;
	DeviceImp di(&rx, &ss);

	Transceiver::ReceiveCycleProfile profile;
	profile.ReceiveStartTime.discriminator = Transceiver::immediateDiscriminator;
	profile.ReceiveStopTime.discriminator = Transceiver::undefinedDiscriminator;
	profile.PacketSize = 1024;
	profile.TuningPreset = 0;
	profile.CarrierFrequency = 700e6;

	Transceiver::ULong i = di.receiveChannel.createReceiveCycleProfile(
		profile.ReceiveStartTime,
		profile.ReceiveStopTime,
		profile.PacketSize,
		profile.TuningPreset,
		profile.CarrierFrequency);

	/*
	while(1) {
		{
			boost::unique_lock<boost::mutex> lock(rx.sample_count_m);
			if(rx.sample_count > 0) {
				break;
			}
		}
	}
	*/

	Transceiver::Time stop(Transceiver::immediateDiscriminator);

	di.receiveChannel.setReceiveStopTime(i, stop);

	di.receiveChannel.wait();
}

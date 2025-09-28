#ifndef IEC_SIMULATION_DATA_GENERATOR
#define IEC_SIMULATION_DATA_GENERATOR

#include <SimulationChannelDescriptor.h>
#include <string>
class IECAnalyzerSettings;

class IECSimulationDataGenerator
{
public:
	IECSimulationDataGenerator();
	~IECSimulationDataGenerator();

	void Initialize( U32 simulation_sample_rate, IECAnalyzerSettings* settings );
	U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channel );

protected:
	IECAnalyzerSettings* mSettings;
	//U32 mSimulationSampleRateHz;

protected:
	//void CreateSerialByte();
	//std::string mSerialText;
	//U32 mStringIndex;

	//SimulationChannelDescriptor mSerialSimulationData;

};
#endif //IEC_SIMULATION_DATA_GENERATOR
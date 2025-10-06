#ifndef IEC_ANALYZER_H
#define IEC_ANALYZER_H

#include <Analyzer.h>
#include "IECAnalyzerSettings.h"
#include "IECAnalyzerResults.h"
#include "IECSimulationDataGenerator.h"
#include <memory>

//#define COMMAND_START_FLAG ( 1 << 0 )
// bits 6 and 7 are reserved

enum class IECFrameType : U8
{
	Default = 0,
	CommandStart = 1,
	EOI = 2,
	Data = 3
};

class ANALYZER_EXPORT IECAnalyzer : public Analyzer2
{
public:
	IECAnalyzer();
	virtual ~IECAnalyzer();

	virtual void SetupResults();
	virtual void WorkerThread();

	virtual U32 GenerateSimulationData( U64 newest_sample_requested, U32 sample_rate, SimulationChannelDescriptor** simulation_channels );
	virtual U32 GetMinimumSampleRateHz();

	virtual const char* GetAnalyzerName() const;
	virtual bool NeedsRerun();

protected: //vars
	IECAnalyzerSettings mSettings;
	std::unique_ptr<IECAnalyzerResults> mResults;
	AnalyzerChannelData* mATN;
	AnalyzerChannelData* mCLK;
	AnalyzerChannelData* mDATA;
	U32 mSampleRateHz;
	bool foundATN;
	bool cmdStartSignaled;
	U64 atnStart;
	U64 eoiStart;
	U64 dataStart;
	U64 data;

	IECSimulationDataGenerator mSimulationDataGenerator;
	bool mSimulationInitilized;

	void markError(U64 sample, Channel& channel);
	void markByteStart(U64 sample);
	void markByteEnd(U64 sample);
	void ReadByte();
	void markCmdStart(U64 endSample);
	void markEOI(U64 endSample);
	void markData(U64 endSample);
	bool checkAtnChange();
	void tick();

private:
	double getTimeUS(U64 sampleStart, U64 sampleEnd)
	{
		return (sampleEnd - sampleStart) * 1000000.0 / mSampleRateHz;
	}
};

extern "C" ANALYZER_EXPORT const char* __cdecl GetAnalyzerName();
extern "C" ANALYZER_EXPORT Analyzer* __cdecl CreateAnalyzer( );
extern "C" ANALYZER_EXPORT void __cdecl DestroyAnalyzer( Analyzer* analyzer );

#endif //IEC_ANALYZER_H

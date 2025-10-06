#include "IECAnalyzer.h"
#include "IECAnalyzerSettings.h"
#include <AnalyzerChannelData.h>

IECAnalyzer::IECAnalyzer()
:	Analyzer2(),  
	mSettings(),
	mSimulationInitilized( false )
{
	SetAnalyzerSettings( &mSettings );
}

IECAnalyzer::~IECAnalyzer()
{
	KillThread();
}

void IECAnalyzer::SetupResults()
{
	// SetupResults is called each time the analyzer is run. Because the same instance can be used for multiple runs, we need to clear the results each time.
	mResults.reset(new IECAnalyzerResults( this, &mSettings ));
	SetAnalyzerResults( mResults.get() );
	mResults->AddChannelBubblesWillAppearOn( mSettings.mDataChannel );
}

void IECAnalyzer::markError(U64 sample, Channel& channel)
{
	mResults->AddMarker(sample, AnalyzerResults::ErrorX, channel);
}

void IECAnalyzer::markByteStart(U64 sample)
{
	mResults->AddMarker(sample, AnalyzerResults::Start, mSettings.mDataChannel);
}

void IECAnalyzer::markByteEnd(U64 sample)
{
	mResults->AddMarker(sample, AnalyzerResults::Stop, mSettings.mDataChannel);
}

void IECAnalyzer::markCmdStart(U64 endSample) 
{
	if (cmdStartSignaled) return;
	cmdStartSignaled = true;

	Frame frame;
	frame.mData1 = 0;
	frame.mFlags = 0;
	frame.mType = (U8)IECFrameType::CommandStart;
	frame.mStartingSampleInclusive = atnStart;
	frame.mEndingSampleInclusive = endSample;

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void IECAnalyzer::markEOI(U64 endSample)
{
	Frame frame;
	frame.mData1 = 0;
	frame.mFlags = 0;
	frame.mType = (U8)IECFrameType::EOI;
	frame.mStartingSampleInclusive = eoiStart;
	frame.mEndingSampleInclusive = endSample;

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

void IECAnalyzer::markData(U64 endSample)
{
	Frame frame;
	frame.mData1 = data;
	frame.mFlags = 0;
	frame.mType = (U8)IECFrameType::Data;
	frame.mStartingSampleInclusive = dataStart;
	frame.mEndingSampleInclusive = endSample;

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );
}

bool IECAnalyzer::checkAtnChange()
{
	if (mATN->GetBitState() == BIT_LOW) 
	{
		if (!foundATN) {
			foundATN = true;
			cmdStartSignaled = false;
			atnStart = mATN->GetSampleNumber();
			return true;
		}
	} else 
	{
		if (foundATN)
		{
			foundATN = false;
			return true;
		}
	}

	return false;
}

void IECAnalyzer::tick()
{
	mCLK->Advance(1);
	mDATA->Advance(1);
	mATN->Advance(1);
}

void IECAnalyzer::WorkerThread()
{
	foundATN = false;
	cmdStartSignaled = false;
	mSampleRateHz = GetSampleRate();
	mATN = GetAnalyzerChannelData( mSettings.mAttentionChannel );
	mCLK = GetAnalyzerChannelData( mSettings.mClockChannel );
	mDATA = GetAnalyzerChannelData( mSettings.mDataChannel );

	// enum MarkerType { Dot, ErrorDot, Square, ErrorSquare, UpArrow, DownArrow, X, ErrorX, Start, Stop, One, Zero };
	// Results->AddMarker( marker_location, AnalyzerResults::Dot, mSettings.mInputChannel );

	while (true)
	{
		checkAtnChange();

		// both lines low indicate getting ready for data transfer
		if (mCLK->GetBitState() == BIT_LOW && mDATA->GetBitState() == BIT_LOW)
		{
			ReadByte();
		}

		tick();
	}
}

void IECAnalyzer::ReadByte()
{
	// Transaction starts once both sender (clk) and receiver (data) are HIGH
	while (mCLK->GetBitState() == BIT_LOW || mDATA->GetBitState() == BIT_LOW)
	{
		if (checkAtnChange()) {
			markByteEnd(mDATA->GetSampleNumber());
			return;
		}
		tick();
	}

	if (foundATN)
		markCmdStart(mATN->GetSampleNumber() - 1);

	// Now check for EOI signal or transmission start
	eoiStart = mDATA->GetSampleNumber();
	while (mCLK->GetBitState() == BIT_HIGH && mDATA->GetBitState() == BIT_HIGH)
	{
		if (checkAtnChange()) {
			markByteEnd(mDATA->GetSampleNumber());
			return;
		}
		tick();
	}

	if (mDATA->GetBitState() == BIT_LOW)
	{
		// EOI. Data MUST go high before clock goes low
		while (mDATA->GetBitState() == BIT_LOW)
		{
			if (checkAtnChange()) {
				markByteEnd(mDATA->GetSampleNumber());
				return;
			}
			if (mCLK->GetBitState() == BIT_LOW) 
			{
				markError(mCLK->GetSampleNumber(), mSettings.mClockChannel);
				return;
			}
			tick();
		}

		// Finally wait for transmission start
		while (mCLK->GetBitState() == BIT_HIGH)
		{
			if (checkAtnChange()) {
				markByteEnd(mDATA->GetSampleNumber());
				return;
			}
			if (mDATA->GetBitState() != BIT_HIGH) 
			{
				markError(mDATA->GetSampleNumber(), mSettings.mDataChannel);
				return;
			}
			tick();
		}

		markEOI(mCLK->GetSampleNumber() - 1);
	}

	markByteStart(mDATA->GetSampleNumber());

	// Start reading bits
	dataStart = mCLK->GetSampleNumber();
	data = 0;
	for (int i = 0; i < 8; i ++) {
		// Wait for clock pulse
		while (mCLK->GetBitState() != BIT_HIGH)
		{
			if (checkAtnChange()) {
				markByteEnd(mDATA->GetSampleNumber());
				return;
			}
			tick();
		}

		// clock pusled, read data pin
		data >>= 1; // LSB gets transmitted first
		if (mDATA->GetBitState() == BIT_HIGH)
		{
			data |= 0x80;
			mResults->AddMarker(mCLK->GetSampleNumber(), AnalyzerResults::One, mSettings.mDataChannel);
		} else {
			//data |= 0;
			mResults->AddMarker(mCLK->GetSampleNumber(), AnalyzerResults::Zero, mSettings.mDataChannel);
		}
		
		// Wait for clock to go LOW again
		while (mCLK->GetBitState() != BIT_LOW)
		{
			if (checkAtnChange()) {
				markByteEnd(mDATA->GetSampleNumber());
				return;
			}
			tick();
		}
	}

	// Wait for DATA to go high 
	while (mDATA->GetBitState() != BIT_HIGH)
	{
		if (checkAtnChange()) {
			markByteEnd(mDATA->GetSampleNumber());
			return;
		}
		if (mCLK->GetBitState() != BIT_LOW) 
		{
			markError(mCLK->GetSampleNumber(), mSettings.mClockChannel);
			return;
		}
		tick();
	}
	
	// Wait for DATA to be pulled low, indicated data received
	while (mDATA->GetBitState() != BIT_LOW)
	{
		if (checkAtnChange()) {
			markByteEnd(mDATA->GetSampleNumber());
			return;
		}
		if (mCLK->GetBitState() != BIT_LOW) 
		{
			markError(mCLK->GetSampleNumber(), mSettings.mClockChannel);
			return;
		}
		tick();
	}

	markData(mCLK->GetSampleNumber());
	markByteEnd(mDATA->GetSampleNumber());
}

bool IECAnalyzer::NeedsRerun()
{
	return false;
}

U32 IECAnalyzer::GenerateSimulationData( U64 minimum_sample_index, U32 device_sample_rate, SimulationChannelDescriptor** simulation_channels )
{
	if( mSimulationInitilized == false )
	{
		mSimulationDataGenerator.Initialize( GetSimulationSampleRate(), &mSettings );
		mSimulationInitilized = true;
	}

	return mSimulationDataGenerator.GenerateSimulationData( minimum_sample_index, device_sample_rate, simulation_channels );
}

U32 IECAnalyzer::GetMinimumSampleRateHz()
{
	return 1000000;
}

const char* IECAnalyzer::GetAnalyzerName() const
{
	return "IEC";
}

const char* GetAnalyzerName()
{
	return "IEC";
}

Analyzer* CreateAnalyzer()
{
	return new IECAnalyzer();
}

void DestroyAnalyzer( Analyzer* analyzer )
{
	delete analyzer;
}
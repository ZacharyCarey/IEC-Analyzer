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

void IECAnalyzer::findBusStart()
{
	// Look for an idle bus (at least 2ms) to start
	// Bus can only be idle while ATN is high
	if (mATN->GetBitState() == BIT_LOW)
	{
		mATN->AdvanceToNextEdge();
		mCLK->AdvanceToAbsPosition(mATN->GetSampleNumber());
		mDATA->AdvanceToAbsPosition(mATN->GetSampleNumber());
	}

	U32 samples_1ms = mSampleRateHz / 1000;
	
	// While ATN is HIGH, keep checking for idle bus.
	// If ATN goes LOw again, we can assume that as the bus starting and start
	// analyzing from there
	while (mATN->GetBitState() == BIT_HIGH)
	{
		if (mCLK->GetBitState() == BIT_HIGH && mDATA->GetBitState() == BIT_HIGH)
		{
			// Bus is idle! Now just verify length.
			U64 lookahead = mATN->GetSampleNumber() + samples_1ms;
			bool clkStaysHigh = !mCLK->WouldAdvancingToAbsPositionCauseTransition(lookahead);
			bool dataStaysHigh = !mDATA->WouldAdvancingToAbsPositionCauseTransition(lookahead);
			bool atnStaysHigh = !mATN->WouldAdvancingToAbsPositionCauseTransition(lookahead);
			if (clkStaysHigh && dataStaysHigh && atnStaysHigh)
			{
				// WOO!
				return;
			} else {
				U64 skip;
				if (!clkStaysHigh) {
					skip = mCLK->GetSampleOfNextEdge();
				} else if(!dataStaysHigh) {
					skip = mDATA->GetSampleOfNextEdge();
				} else if (!atnStaysHigh)
				{
					skip = mATN->GetSampleOfNextEdge();
				} else {
					// throw error?
				}
				mATN->AdvanceToAbsPosition(skip);
				mDATA->AdvanceToAbsPosition(skip);
				mCLK->AdvanceToAbsPosition(skip);
			}
		} else {
			// Tick forward to keep looking
			mATN->Advance(1);
			mCLK->Advance(1);
			mDATA->Advance(1);
		}
	}

	// ATN went low, return now to start analyzing
}

void IECAnalyzer::WorkerThread()
{
	mSampleRateHz = GetSampleRate();
	mATN = GetAnalyzerChannelData( mSettings.mAttentionChannel );
	mCLK = GetAnalyzerChannelData( mSettings.mClockChannel );
	mDATA = GetAnalyzerChannelData( mSettings.mDataChannel );

	// enum MarkerType { Dot, ErrorDot, Square, ErrorSquare, UpArrow, DownArrow, X, ErrorX, Start, Stop, One, Zero };
	// Results->AddMarker( marker_location, AnalyzerResults::Dot, mSettings.mInputChannel );

	while (true)
	{
		// Find either idle bus or start of a new CMD
		findBusStart();

		if (mATN->GetBitState() == BIT_HIGH)
		{
			// go to transaction start
			mATN->AdvanceToNextEdge();
		}

		TransactionStart();
		
		//mResults->AddMarker(cmdStart, AnalyzerResults::Start, mSettings.mAttentionChannel);
		//mResults->AddMarker(cmdEnd, AnalyzerResults::Stop, mSettings.mAttentionChannel);

		//ReportProgress( cmdEnd );
	}

	/*if( mSerial->GetBitState() == BIT_LOW )
		mSerial->AdvanceToNextEdge();

	U32 samples_per_bit = mSampleRateHz / mSettings.mBitRate;
	U32 samples_to_first_center_of_first_data_bit = U32( 1.5 * double( mSampleRateHz ) / double( mSettings.mBitRate ) );

	for( ; ; )
	{
		U8 data = 0;
		U8 mask = 1 << 7;
		
		mSerial->AdvanceToNextEdge(); //falling edge -- beginning of the start bit

		U64 starting_sample = mSerial->GetSampleNumber();

		mSerial->Advance( samples_to_first_center_of_first_data_bit );

		for( U32 i=0; i<8; i++ )
		{
			//let's put a dot exactly where we sample this bit:
			mResults->AddMarker( mSerial->GetSampleNumber(), AnalyzerResults::Dot, mSettings.mInputChannel );

			if( mSerial->GetBitState() == BIT_HIGH )
				data |= mask;

			mSerial->Advance( samples_per_bit );

			mask = mask >> 1;
		}


		//we have a byte to save. 
		Frame frame;
		frame.mData1 = data;
		frame.mFlags = 0;
		frame.mStartingSampleInclusive = starting_sample;
		frame.mEndingSampleInclusive = mSerial->GetSampleNumber();

		mResults->AddFrame( frame );
		mResults->CommitResults();
		ReportProgress( frame.mEndingSampleInclusive );
	}*/
}

void IECAnalyzer::TransactionStart()
{
	bool isEOI;

	// ATN went low to start the transaction
	U64 start = mATN->GetSampleNumber();
	mCLK->AdvanceToAbsPosition(start);
	mDATA->AdvanceToAbsPosition(start);

	// Wait for sender (clk) and receiver (data) to respond by pulling LOW
	while (mCLK->GetBitState() == BIT_HIGH || mDATA->GetBitState() == BIT_HIGH)
	{
		mATN->Advance(1);
		mCLK->Advance(1);
		mDATA->Advance(1);
		// Verify ATN is still in valid state
		if (mATN->GetBitState() == BIT_HIGH)
		{
			// Error
			markError(mATN->GetSampleNumber(), mSettings.mAttentionChannel);
			return;
		}
	}

	// Command is ready to send
	Frame frame;
	frame.mData1 = 0;
	frame.mFlags = 0;
	frame.mType = (U8)IECFrameType::CommandStart;
	frame.mStartingSampleInclusive = start;
	frame.mEndingSampleInclusive = mCLK->GetSampleOfNextEdge();

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	// Get time when CMD ends
	mATN->AdvanceToNextEdge();
	while (mCLK->WouldAdvancingToAbsPositionCauseTransition(mATN->GetSampleNumber()))
	{
		if (!ReadByte(&isEOI, true)) return;
	}

	// Read all command bytes, move everything to end of ATN
	mCLK->AdvanceToAbsPosition(mATN->GetSampleNumber());
	mDATA->AdvanceToAbsPosition(mATN->GetSampleNumber());

	isEOI = false;

	// Attempt to read "normal" bytes. If it fails, it's likely because
	// ATN was started again
	while(ReadByte(&isEOI, false)) {
		
	}
}

// Called on the rising edge of DATA
// TODO need to check for attention interrupts during normal bytes
bool IECAnalyzer::ReadByte(bool* isEOI, bool isCMD)
{
	// Transaction starts once both sender (clk) and receiver (data) are HIGH
	while (mCLK->GetBitState() == BIT_LOW || mDATA->GetBitState() == BIT_LOW)
	{
		mCLK->Advance(1);
		mDATA->Advance(1);

		if (!isCMD) {
			mATN->AdvanceToAbsPosition(mDATA->GetSampleNumber());
			if (mATN->GetBitState() == BIT_LOW) {
				// Begin new transaction
				TransactionStart();
				return false;
			}
		}
	}

	// Check for EOI
	mCLK->AdvanceToNextEdge();
	double eoiSignalLength = getTimeUS(mDATA->GetSampleNumber(), mCLK->GetSampleNumber());
	*isEOI = false;
	if (eoiSignalLength >= 200) {
		// EOI detected
		*isEOI = true;

		if (isCMD)
		{
			// EOI shouldnt happen during CMD
			markError(mCLK->GetSampleNumber(), mSettings.mClockChannel);
			return false;
		}

		U64 eoiStart = mDATA->GetSampleNumber();

		// DATA will go low then high again
		mDATA->AdvanceToNextEdge();
		if (mDATA->GetBitState() != BIT_LOW || mDATA->GetSampleNumber() >= mCLK->GetSampleNumber()) {
			markError(mDATA->GetSampleNumber(), mSettings.mDataChannel);
			return false;
		}
		U64 eoiAckStart = mDATA->GetSampleNumber();
		mDATA->AdvanceToNextEdge();
		double eoiAckTime = getTimeUS(eoiAckStart, mDATA->GetSampleNumber()); // Must be at last 60us
		if (mDATA->GetBitState() != BIT_HIGH || mDATA->GetSampleNumber() >= mCLK->GetSampleNumber() || eoiAckTime < 60.0){
			markError(mDATA->GetSampleNumber(), mSettings.mDataChannel);
			return false;
		}

		Frame frameEOI;
		frameEOI.mData1 = 0;
		frameEOI.mFlags = 0;
		frameEOI.mType = (U8)IECFrameType::EOI;
		frameEOI.mStartingSampleInclusive = eoiStart;
		frameEOI.mEndingSampleInclusive = mCLK->GetSampleNumber();

		mResults->AddFrame( frameEOI );
		mResults->CommitResults();
		ReportProgress( frameEOI.mEndingSampleInclusive );
	}

	U64 dataStart = mCLK->GetSampleNumber() + 1;
	U64 data = 0;
	for (int i = 0; i < 8; i ++) {
		// Wait for high pulse
		mCLK->AdvanceToNextEdge();
		mDATA->AdvanceToAbsPosition(mCLK->GetSampleNumber());
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
		mCLK->AdvanceToNextEdge();
	}

	// Wait for DATA to go high 
	mDATA->AdvanceToAbsPosition(mCLK->GetSampleNumber());
	while (mDATA->GetBitState() != BIT_HIGH) mDATA->AdvanceToNextEdge();
	
	// Wait for DATA to be pulled low, indicated data received
	mDATA->AdvanceToNextEdge();
	if (mCLK->WouldAdvancingToAbsPositionCauseTransition(mDATA->GetSampleNumber())){
		mCLK->AdvanceToNextEdge();
		markError(mCLK->GetSampleNumber(), mSettings.mClockChannel);
		return false;
	}

	// sync samples
	mCLK->AdvanceToAbsPosition(mDATA->GetSampleNumber()); 
	if (!isCMD) {
		mATN->AdvanceToAbsPosition(mDATA->GetSampleNumber());
	}

	// Save transmitted data
	Frame frame;
	frame.mData1 = data;
	frame.mFlags = 0;
	frame.mType = (U8)IECFrameType::Command;
	frame.mStartingSampleInclusive = dataStart;
	frame.mEndingSampleInclusive = mDATA->GetSampleNumber();

	mResults->AddFrame( frame );
	mResults->CommitResults();
	ReportProgress( frame.mEndingSampleInclusive );

	return true;
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
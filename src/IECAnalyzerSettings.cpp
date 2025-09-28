#include "IECAnalyzerSettings.h"
#include <AnalyzerHelpers.h>


IECAnalyzerSettings::IECAnalyzerSettings()
:	mDataChannel( UNDEFINED_CHANNEL ),
	mClockChannel( UNDEFINED_CHANNEL ),
	mAttentionChannel( UNDEFINED_CHANNEL ),
	//mBitRate( 9600 ),
	mDataChannelInterface(),
	mClockChannelInterface(),
	mAttentionChannelInterface()
	//mBitRateInterface()
{
	mAttentionChannelInterface.SetTitleAndTooltip( "ATN", "Attention Channel" );
	mAttentionChannelInterface.SetChannel( mAttentionChannel );

	mClockChannelInterface.SetTitleAndTooltip( "CLK", "Clock Channel" );
	mClockChannelInterface.SetChannel( mClockChannel );

	mDataChannelInterface.SetTitleAndTooltip( "DATA", "Data Channel" );
	mDataChannelInterface.SetChannel( mDataChannel );

	//mBitRateInterface.SetTitleAndTooltip( "Bit Rate (Bits/S)",  "Specify the bit rate in bits per second." );
	//mBitRateInterface.SetMax( 6000000 );
	//mBitRateInterface.SetMin( 1 );
	//mBitRateInterface.SetInteger( mBitRate );

	AddInterface( &mAttentionChannelInterface );
	AddInterface( &mClockChannelInterface );
	AddInterface( &mDataChannelInterface );
	//AddInterface( &mBitRateInterface );

	AddExportOption( 0, "Export as text/csv file" );
	AddExportExtension( 0, "text", "txt" );
	AddExportExtension( 0, "csv", "csv" );

	ClearChannels();
	AddChannel( mAttentionChannel, "ATN", false );
	AddChannel( mDataChannel, "DATA", false );
	AddChannel( mClockChannel, "CLK", false );
}

IECAnalyzerSettings::~IECAnalyzerSettings()
{
}

bool IECAnalyzerSettings::SetSettingsFromInterfaces()
{
	mDataChannel = mDataChannelInterface.GetChannel();
	mAttentionChannel = mAttentionChannelInterface.GetChannel();
	mClockChannel = mClockChannelInterface.GetChannel();
	//mBitRate = mBitRateInterface.GetInteger();

	ClearChannels();
	AddChannel( mDataChannel, "DATA", true );
	AddChannel( mAttentionChannel, "ATN", true);
	AddChannel( mClockChannel, "CLK", true);

	return true;
}

void IECAnalyzerSettings::UpdateInterfacesFromSettings()
{
	mDataChannelInterface.SetChannel( mDataChannel );
	mAttentionChannelInterface.SetChannel( mAttentionChannel );
	mClockChannelInterface.SetChannel( mClockChannel );
	//mBitRateInterface.SetInteger( mBitRate );
}

void IECAnalyzerSettings::LoadSettings( const char* settings )
{
	SimpleArchive text_archive;
	text_archive.SetString( settings );

	text_archive >> mDataChannel;
	text_archive >> mClockChannel;
	text_archive >> mAttentionChannel;
	//text_archive >> mBitRate;

	ClearChannels();
	AddChannel( mDataChannel, "DATA", true );
	AddChannel( mAttentionChannel, "ATN", true);
	AddChannel( mClockChannel, "CLK", true );

	UpdateInterfacesFromSettings();
}

const char* IECAnalyzerSettings::SaveSettings()
{
	SimpleArchive text_archive;

	text_archive << mDataChannel;
	text_archive << mClockChannel;
	text_archive << mAttentionChannel;
	//text_archive << mBitRate;

	return SetReturnString( text_archive.GetString() );
}

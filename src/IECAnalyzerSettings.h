#ifndef IEC_ANALYZER_SETTINGS
#define IEC_ANALYZER_SETTINGS

#include <AnalyzerSettings.h>
#include <AnalyzerTypes.h>

class IECAnalyzerSettings : public AnalyzerSettings
{
public:
	IECAnalyzerSettings();
	virtual ~IECAnalyzerSettings();

	virtual bool SetSettingsFromInterfaces();
	void UpdateInterfacesFromSettings();
	virtual void LoadSettings( const char* settings );
	virtual const char* SaveSettings();

	
	Channel mDataChannel;
	Channel mClockChannel;
	Channel mAttentionChannel;
	//U32 mBitRate;

protected:
	AnalyzerSettingInterfaceChannel	mDataChannelInterface;
	AnalyzerSettingInterfaceChannel	mClockChannelInterface;
	AnalyzerSettingInterfaceChannel	mAttentionChannelInterface;
	//AnalyzerSettingInterfaceInteger	mBitRateInterface;
};

#endif //IEC_ANALYZER_SETTINGS

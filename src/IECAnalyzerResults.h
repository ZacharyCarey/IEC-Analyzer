#ifndef IEC_ANALYZER_RESULTS
#define IEC_ANALYZER_RESULTS

#include <AnalyzerResults.h>

class IECAnalyzer;
class IECAnalyzerSettings;

class IECAnalyzerResults : public AnalyzerResults
{
public:
	IECAnalyzerResults( IECAnalyzer* analyzer, IECAnalyzerSettings* settings );
	virtual ~IECAnalyzerResults();

	virtual void GenerateBubbleText( U64 frame_index, Channel& channel, DisplayBase display_base );
	virtual void GenerateExportFile( const char* file, DisplayBase display_base, U32 export_type_user_id );

	virtual void GenerateFrameTabularText(U64 frame_index, DisplayBase display_base );
	virtual void GeneratePacketTabularText( U64 packet_id, DisplayBase display_base );
	virtual void GenerateTransactionTabularText( U64 transaction_id, DisplayBase display_base );

protected: //functions

protected:  //vars
	IECAnalyzerSettings* mSettings;
	IECAnalyzer* mAnalyzer;
};

#endif //IEC_ANALYZER_RESULTS

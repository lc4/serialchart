#include "mainwindow.h"
#include "decoderbase.h"
#include "decodercsv.h"
#include "decoderhdlc.h"
//#include "decoderplugin.h"


DecoderBase::DecoderBase(QObject *parent, Configuration* config) :
    QObject(parent)
{
    this->config = config;
    packetMaxLen = config->get("_setup_","packet_max_len","256").toInt();
    packetStarted = false;
}


DecoderBase* createDecoder(QObject *parent, Configuration* config){

	QString qs = config->get("_setup_","decoder");
	if(qs == "hdlc")
        return new DecoderHdlc(parent,config);
	else if(qs == "bin")
        return new DecoderBin(parent,config);
//    else if(qs == "plugin")
//        return new DecoderPlugin(parent,config);

    //default
        return new DecoderCsv(parent,config);
}



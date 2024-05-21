#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "decoderbase.h"
//#include "plugin.h"
//#include "QWebFrame"
#include <QFileInfo>

bool fileExists(QString path) {
	QFileInfo check_file(path);
	// check if path exists and if yes: Is it really a file and no directory?
	return check_file.exists() && check_file.isFile();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    documentIsDirty = false;

    config = new Configuration(this);
    portValid = false;

    setWindowTitle(APP_NAME " " APP_VERSION);
	loadDocument(settings.value("Last Configuration").toString());
    restoreGeometry(settings.value("WINDOW_GEOMETRY").toByteArray());
    restoreState(settings.value("DOCK_LOCATIONS").toByteArray());

}


void MainWindow::showEvent(QShowEvent *e)
{
    QMainWindow::showEvent(e);
    //Update View menu checkboxes
    ui->actionToolbar->setChecked(ui->mainToolBar->isVisible());
    ui->actionConfiguration->setChecked(ui->dockWidgetConfiguration->isVisible());
    ui->actionChart->setChecked(ui->dockWidgetChart->isVisible());
//    ui->actionWebView->setChecked(ui->dockWidgetWebView->isVisible());
}


MainWindow::~MainWindow()
{
    settings.setValue("WINDOW_GEOMETRY", saveGeometry());
	settings.setValue("DOCK_LOCATIONS", saveState());

    delete ui;
}

void MainWindow::changeEvent(QEvent *e)
{
    QMainWindow::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}


void MainWindow::closeEvent(QCloseEvent *e)
{
    if(portValid) port->requestToStop();
    if(!checkDocument()) e->ignore();
}



void MainWindow::on_actionNew_triggered()
{
    if(checkDocument()){
        ui->configurationText->clear();
        documentFilePath = "";
        ui->statusBar->showMessage(documentFilePath);
        documentIsDirty  = false;
    }
}

void MainWindow::on_actionExit_triggered()
{
    this->close();
}


void MainWindow::on_actionOpen_triggered()
{
    if(!checkDocument()) return;

    QString filePath = QFileDialog::getOpenFileName(this,
        tr("Open File"), documentFilePath, tr("SerialChart Configuration (*.scc);;All Files (*.*)"));

    if(!filePath.isEmpty()) loadDocument(filePath);

}

void MainWindow::on_actionSave_triggered()
{
	actionSave();
}

void MainWindow::on_actionSaveAs_triggered()
{
   actionSaveAs();
}




/**************************************************************************************************/
/**************************************************************************************************/

bool MainWindow::actionSave()
{
	if(documentFilePath.isEmpty())
		return actionSaveAs();
	else
		return saveDocument(documentFilePath);
}

bool MainWindow::actionSaveAs()
{
	QString filePath = QFileDialog::getSaveFileName(this, tr("Save File"),
							   documentFilePath,
							   tr("SerialChart Configuration (*.scc);;All Files (*.*)"));
	if(!filePath.isEmpty())
		return saveDocument(filePath);
	return false;
}

bool MainWindow::checkDocument()
{
    if(documentIsDirty)
	{
		int ret = QMessageBox::warning(0,"",
			"Current configuration was changed,\nSave file first?",
			QMessageBox::Save,QMessageBox::Discard, QMessageBox::Cancel|QMessageBox::Default);

		if(ret == QMessageBox::Save)
			return actionSave();
		else if(ret == QMessageBox::Discard)
			return true;
		else //if(ret == QMessageBox::Cancel)
			return false;
	}
    return true;
}

bool MainWindow::saveDocument(const QString& filePath)
{
    bool success = false;
    QFile file(filePath);
    if(file.open(QIODevice::WriteOnly)){
        QTextStream textStream(&file);
        textStream<<ui->configurationText->toPlainText();
        textStream.flush();
        documentIsDirty  = false;
        updateDocumentFilePath(filePath);
        success = true;
    }

    if(!success)
        QMessageBox::critical(0,"","Could not save file: " + filePath);
    else
        ui->statusBar->showMessage("File Saved",2000);

    return success;
}

bool MainWindow::loadDocument(const QString& filePath)
{
	if(!fileExists(filePath))
		return false;

	bool success = false;
	QFile file(filePath);
	if(file.open(QIODevice::ReadOnly)){
		QTextStream textStream(&file);
		ui->configurationText->setPlainText(textStream.readAll());
		updateDocumentFilePath(filePath);
		success = true;
	}
	if(!success) QMessageBox::critical(0,"","Could not load file: " + filePath);
    return success;
}

void MainWindow::updateDocumentFilePath(const QString& filePath){
    documentFilePath = filePath;
    settings.setValue("Last Configuration",filePath);
    this->setWindowTitle(APP_NAME " " APP_VERSION " - " + QFileInfo(filePath).fileName() );
    documentIsDirty  = false;
};


void MainWindow::on_configurationText_textChanged()
{
    documentIsDirty = true;
}

void MainWindow::on_actionAbout_triggered()
{
	QMessageBox::about(0,"",
		APP_NAME " ver. " APP_VERSION "\n"\
		"By Vincent Lee\n"	\
		"Based on SerialChart by Sergiu Baluta, www.Starlino.com");
}

void MainWindow::on_actionRun_triggered()
{
    config->parse( ui->configurationText->toPlainText());

    //dataText
    ui->dataText->setMaximumBlockCount(MAX(1,config->get("_setup_","width").toInt()));
    ui->dataText->clear();

    //chart
    ui->chart->init(config);

    //actions
    ui->actionRun->setEnabled(false);
    ui->actionStop->setEnabled(true);
    ui->sendButton->setEnabled(true);

    //port
    port = createPort(this,config);
    decoder = createDecoder(this,config);
    display = createDisplay(this,config);
    portValid = true;

    //port signals
	connect(port,SIGNAL(newData(QByteArray)),decoder,SLOT(newData(QByteArray)));
    connect(port,SIGNAL(stopped()),this,SLOT(portStopped()));
	connect(port,SIGNAL(message(QString,QString)),this,SLOT(message(QString,QString)));

    //decoder signals
	connect(decoder,SIGNAL(newPacket(DecoderBase*)),ui->chart,SLOT(newPacket(DecoderBase*)));
	connect(decoder,SIGNAL(newPacket(DecoderBase*)),display,SLOT(newPacket(DecoderBase*)));

    //display signals
	connect(display,SIGNAL(newDisplay(QString)),ui->dataText,SLOT(appendPlainText(QString)));

    //Startup send
    QString send_run = config->get("_setup_","send_run","").trimmed();
	if("" != send_run)
	{
        port->forceSend = true; //append to send buffer even if port is not running
        sendString(send_run);
        port->forceSend = false;
    }

    port->start();
}

void MainWindow::on_actionStop_triggered()
{
    QString send_stop = config->get("_setup_","send_stop","").trimmed();
    if("" != send_stop){
        port->forceSend = true; //append to send buffer even if port is not running
        sendString(send_stop);
        port->forceSend = false;
    }

    ui->actionStop->setEnabled(false);
    ui->sendButton->setEnabled(false);

    port->requestToStop();
}

void MainWindow::portStopped(){
    ui->actionStop->setEnabled(false);
    ui->sendButton->setEnabled(false);
    ui->actionRun->setEnabled(true);
    port->deleteLater();
    portValid = false;
    delete decoder;
    delete display;
}

void MainWindow::on_actionToolbar_toggled(bool b)
{
    ui->mainToolBar->setVisible(b);
}

void MainWindow::on_actionChart_toggled(bool b)
{
    ui->dockWidgetChart->setVisible(b);
}

void MainWindow::on_actionConfiguration_toggled(bool b)
{
    ui->dockWidgetConfiguration->setVisible(b);
}


void MainWindow::on_sendButton_clicked(){
    sendString( ui->sendText->text());
}

bool MainWindow::sendString(QString str){
    if(portValid){
        if(ui->checkBoxCR->isChecked()) str.append('\r');
        if(ui->checkBoxLF->isChecked()) str.append('\n');
        QByteArray data = stringLiteralUnescape(str.toLatin1());
        if(ui->checkBoxEcho->isChecked()) ui->dataText->appendPlainText(data);
        port->send(data);
    }
    return portValid;
}



void MainWindow::message(const QString& text,const QString& type)
{
    if("critical"==type) QMessageBox::critical(0,"",text);
    else QMessageBox::information(0,"",text);
}

void MainWindow::on_sendText_returnPressed()
{
    on_sendButton_clicked();
}

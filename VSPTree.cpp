#include "VSPTree.h"
#include <QTest>

#define VERSION "VSPTree 0.5"
#define PATH32 "C:\\Program Files\\Microsoft Visual Studio 11.0\\Team Tools\\Performance Tools\\"
#define PATH64 "C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\Team Tools\\Performance Tools\\"

VSPTree::VSPTree( QString appPath,  QString argument) :
	m_applicationPath( appPath ),
	m_settings( appPath + "/VSPTree.ini", QSettings::IniFormat )
{ 
	QDir dir(PATH32);
	QDir dir2(PATH64);
	if ( dir.exists() )
	{
		m_profilerpath = PATH32;
	}
	else if ( dir2.exists() )
	{
		m_profilerpath = PATH64;
	}
	else
	{
		QMessageBox::critical(NULL,"Standalone Debugger not installed", "C:\\Program Files (x86)\\Microsoft Visual Studio 11.0\\Team Tools\\Performance Tools\\ does not exist");
	}

	QStringList env = QProcess::systemEnvironment();
	env.replaceInStrings(QRegExp("^PATH=(.*)", Qt::CaseInsensitive), QString("PATH=\\1;") + m_profilerpath);
	m_process.setEnvironment(env);

	if ( m_settings.contains("m_lastExe") ) 
	{
		m_lastExe = m_settings.value("m_lastExe").toString();
	}

	if ( m_settings.contains("m_lastVSP") ) 
	{
		m_lastVSP = m_settings.value("m_lastVSP").toString();
	}

	if ( m_settings.contains("m_lastArguments") ) 
	{
		m_lastArguments = m_settings.value("m_lastArguments").toString();
	}

	m_editorPath = "";
	if ( m_settings.contains("m_editorPath") ) 
	{
		m_editorPath = m_settings.value("m_editorPath").toString();
	}

	m_editorArguments = "<filename> -n<linenum>";
	if ( m_settings.contains("m_editorArguments") ) 
	{
		m_editorArguments = m_settings.value("m_editorArguments").toString();
	}

	m_log = new QTextEdit();
	m_log->setReadOnly(true);
	connect (&m_process, SIGNAL(readyReadStandardOutput()),this,  SLOT(StdOutLog()));
	connect (&m_process, SIGNAL(readyReadStandardError()),this,  SLOT(StdErrorLog()));

	menubar = this->menuBar();
	QMenu* fileMenu = menubar->addMenu("File");
	QAction* openAct = fileMenu->addAction("Open VSP");
	connect(openAct, SIGNAL(triggered()), this, SLOT(LoadVSPClicked()));
	QAction* exitAct = fileMenu->addAction("Exit");
	connect(exitAct, SIGNAL(triggered()), this, SLOT(close()));
	
	QMenu* profilerMenu = menubar->addMenu("Profiler");

	QAction* startProfilingAct = profilerMenu->addAction("Start Profiler Service");
	connect(startProfilingAct, SIGNAL(triggered()), this, SLOT(StartProfiling()));

	QAction* ProfileAct = profilerMenu->addAction("Profile Application");
	connect(ProfileAct, SIGNAL(triggered()), this, SLOT(Profile()));

	QAction* stopProfilingAct = profilerMenu->addAction("Stop Profiler Service");
	connect(stopProfilingAct, SIGNAL(triggered()), this, SLOT(StopProfiling()));

	QMenu* OptionsMenu = menubar->addMenu("Options");
	QAction* setTextEditor = OptionsMenu->addAction("Set Text Editor");
	connect(setTextEditor, SIGNAL(triggered()), this, SLOT(setTextEditor()));

	m_tabWidget = new QTabWidget();

	m_callTreeWidget = new QTreeWidget();

	m_flatCallTreeWidget = new QTreeWidget();

	m_functionSummaryTreeWidget = new QTreeWidget();

	m_tabWidget->addTab(m_callTreeWidget,"CallTree");
	m_tabWidget->addTab(m_flatCallTreeWidget,"CallTreeFlat");
	m_tabWidget->addTab(m_functionSummaryTreeWidget,"FunctionSummary");
	m_tabWidget->addTab(m_log,"Log");

	m_callTreeMenu = new QMenu();
	QAction* openFileAct = m_callTreeMenu->addAction("Open File");
	connect(openFileAct, SIGNAL(triggered()), this, SLOT(openFile()));

	QAction* expandAllAct = m_callTreeMenu->addAction("Expand All");
	connect(expandAllAct, SIGNAL(triggered()), m_callTreeWidget, SLOT(expandAll()));

	QAction* collapseAllAct = m_callTreeMenu->addAction("Collapse All");
	connect(collapseAllAct, SIGNAL(triggered()), m_callTreeWidget, SLOT(collapseAll()));

	m_flatCallTreeMenu = new QMenu();
	
	m_flatCallTreeMenu->addAction(openFileAct);
	QAction* FindInTreeAct = m_flatCallTreeMenu->addAction("Find In Tree");
	connect(FindInTreeAct, SIGNAL(triggered()), this, SLOT(FindInTree()));

	//threads
	connect( &m_thread, SIGNAL( output( QString ) ), this, SLOT( Log( QString ) ) );
	connect( &m_thread, SIGNAL( finished() ), this, SLOT( workDone() ) );

	this->setCentralWidget(m_tabWidget);
	this->setWindowTitle(VERSION);
	this->resize(640,480);
	this->show();

	if ( !argument.isEmpty() )
	{
		LoadVSP(argument);
	}
}

VSPTree::~VSPTree()
{ 

}

void VSPTree::LoadVSPClicked()
{
	QString fileName = QFileDialog::getOpenFileName(0,"VSP to open",m_lastVSP,"*.vspx");
	if ( !fileName.isNull() )
	{
		LoadVSP(fileName);
	}
}

void VSPTree::LoadVSP(QString fileName)
{
	m_callTreeWidget->clear();
	m_flatCallTreeWidget->clear();
	m_functionSummaryTreeWidget->clear();

	GenerateReport(fileName);
	m_lastVSP = fileName;
}

void VSPTree::LoadCallTree(QString callTreeSummary)
{
	Log("Reading : " + callTreeSummary);
	QDomDocument doc = readXML( callTreeSummary );

	QTreeWidgetItem* root = new QTreeWidgetItem(m_callTreeWidget);
	root->setText(0,"CallTree");
	m_callTreeWidget->insertTopLevelItem(0, root);

	QTreeWidgetItem* flatroot = new QTreeWidgetItem(m_flatCallTreeWidget);
	flatroot->setText(0,"FlatCallTree");
	m_callTreeWidget->insertTopLevelItem(0, flatroot);

	QDomNodeList functionList = doc.elementsByTagName("CallTree");
	
	QHash<int, QTreeWidgetItem*> levelHash;

	levelHash.insert(0,root);

	QStringList fields;
	fields << "FunctionName" << "InclSamples" << "ExclSamples" << "InclSamplesPercent"
		<< "ExclSamplesPercent" << "SourceFile" << "LineNumber" << "ModuleName" << "ModulePath" 
		<< "FunctionAddress" << "Level" << "ProcessName" << "PID"; 

	
	m_callTreeWidget->setColumnCount(fields.count());
	m_callTreeWidget->setHeaderLabels(fields);
	
	m_flatCallTreeWidget->setColumnCount(fields.count());
	m_flatCallTreeWidget->setHeaderLabels(fields);

	for ( int i = 0; i < functionList.count(); i++ )
	{
		QTreeWidgetItem* tempWidgetItem = new QTreeWidgetItem();
		for ( int j = 0; j < fields.count(); j++ )
		{
			QString cellText = functionList.at(i).toElement().attribute( fields.at(j) );
			if ( fields.at(j) == "ExclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "InclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "Level" && cellText.count() < 2)
			{
				cellText.prepend('0');
			}
			tempWidgetItem->setText( j, cellText );
		}
		int level = functionList.at(i).toElement().attribute("Level").toInt();
		levelHash.value(level)->addChild(tempWidgetItem);
		QTreeWidgetItem* flatTreeWidgetItem = tempWidgetItem->clone();
		flatroot->addChild(flatTreeWidgetItem);
		m_flatHash.insert(flatTreeWidgetItem,tempWidgetItem);
		levelHash.insert(level+1,tempWidgetItem);
	}

	m_callTreeWidget->expandAll();
	m_callTreeWidget->setAlternatingRowColors(true);
	m_flatCallTreeWidget->expandAll();
	m_flatCallTreeWidget->setAlternatingRowColors(true);
	m_flatCallTreeWidget->setSortingEnabled(true);
}

void VSPTree::LoadFunctionSummary(QString functionSummary)
{
	Log("Reading : " + functionSummary);
	QDomDocument doc = readXML( functionSummary );

	QTreeWidgetItem* root = new QTreeWidgetItem(m_functionSummaryTreeWidget);
	root->setText(0,"FunctionSummary");
	m_functionSummaryTreeWidget->insertTopLevelItem(0, root);

	QDomNodeList functionList = doc.elementsByTagName("Function");

	QStringList fields;
	fields << "FunctionName" << "InclSamples" << "ExclSamples" << "InclSamplesPercent"
		<< "ExclSamplesPercent" << "SourceFile" << "LineNumber" << "ModuleName" << "ModulePath" 
		<< "FunctionAddress" << "ProcessName" << "PID"; 

	m_functionSummaryTreeWidget->setColumnCount(fields.count());
	m_functionSummaryTreeWidget->setHeaderLabels(fields);

	for ( int i = 0; i < functionList.count(); i++ )
	{
		QTreeWidgetItem* tempWidgetItem = new QTreeWidgetItem();
		for ( int j = 0; j < fields.count(); j++ )
		{
			QString cellText = functionList.at(i).toElement().attribute( fields.at(j) );
			if ( fields.at(j) == "ExclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "InclSamplesPercent" && cellText.count() < 5)
			{
				cellText.prepend('0');
			}
			if ( fields.at(j) == "Level" && cellText.count() < 2)
			{
				cellText.prepend('0');
			}
			tempWidgetItem->setText( j, cellText );
		}
		root->addChild(tempWidgetItem);
	}

	m_functionSummaryTreeWidget->expandAll();
	m_functionSummaryTreeWidget->setAlternatingRowColors(true);
	m_functionSummaryTreeWidget->setSortingEnabled(true);

}

void VSPTree::contextMenuEvent ( QContextMenuEvent * event ) 
{
	if ( m_tabWidget->currentWidget() == m_callTreeWidget )
	{
		m_callTreeMenu->exec(QCursor::pos());
	}
	else if ( m_tabWidget->currentWidget() == m_flatCallTreeWidget ) 
	{
		m_flatCallTreeMenu->exec(QCursor::pos());
	}
	event->accept();
}

void VSPTree::GenerateReport(QString fileName)
{
	m_tabWidget->setCurrentWidget(m_log);
	m_thread.m_fileName = fileName;
	m_thread.m_profilerpath = m_profilerpath;
	m_tabWidget->setEnabled(false);
	menubar->setEnabled(false);
	m_thread.start();
}

void VSPTree::StopProfiling()
{
	m_tabWidget->setCurrentWidget(m_log);
	m_process.start(m_profilerpath + QString("VsPerfCmd.exe"), QStringList() << "-shutdown" );
	m_process.waitForFinished();
}

void VSPTree::StartProfiling()
{
	m_tabWidget->setCurrentWidget(m_log);
	QStringList args = QStringList() << "VsPerfMon.exe" << "/SAMPLE" << "/OUTPUT:" + QApplication::applicationDirPath() + "/dummy.vsp" ;
	writeText("StartProfiling.bat", QString("SET PATH=%PATH%;") + m_profilerpath + QString("\n") + args.join(" ") );
	m_process.startDetached("StartProfiling.bat");
}

void VSPTree::Profile()
{
	QString fileName = QFileDialog::getOpenFileName(0,"Executable to Profile",m_lastExe,"*.exe");
	if ( fileName.isNull() ) 
	{
		return;
	}

	m_lastExe = fileName;

	bool ok;
	QString argumentstext = QInputDialog::getText(this, "Arguments", "Arguments to pass to executable. Leave blank or cancel for nothing.", QLineEdit::Normal, m_lastArguments, &ok);
	if (ok)
	{
		m_lastArguments = argumentstext;
	}
	else
	{
		return;
	}

	QString reportfileName = QFileDialog::getSaveFileName(0,"Report to save",m_lastVSP,"*.vspx");
	if ( !fileName.isNull() && !reportfileName.isNull() )
	{
		m_lastVSP = reportfileName;
		m_tabWidget->setCurrentWidget(m_log);
		QStringList args;
		args << "VsPerf.exe" 
			<< QString("/launch:\"") + fileName + QString("\"")
			<< QString("/file:\"") + reportfileName + QString("\"") 
			;
		if( !argumentstext.isNull() )
		{
			args << QString("/args:\"") + argumentstext + QString("\"");
		}
		Log(args.join(" "));
		writeText("Profile.bat", 
			"cd /d " + QFileInfo(fileName).absolutePath() + QString("\n") + QString("SET PATH=%PATH%;") + m_profilerpath + QString("\n") + args.join(" ")
			);
		m_process.startDetached("Profile.bat",args);
	}
}


void VSPTree::Log(QString text)
{
	m_log->append(text);
}

void VSPTree::StdOutLog()
{
	Log(QString(m_process.readAllStandardOutput().data()));
}

void VSPTree::StdErrorLog()
{
	Log(QString(m_process.readAllStandardError().data()));
}

void VSPTree::FindInTree()
{
	m_callTreeWidget->setCurrentItem(m_flatHash.value(m_flatCallTreeWidget->currentItem()));
	m_tabWidget->setCurrentWidget(m_callTreeWidget);
}

void VSPTree::openFile()
{
	if (m_editorPath.isEmpty())
	{
		setTextEditor();
	}
	if ( m_tabWidget->currentWidget() == m_callTreeWidget || m_tabWidget->currentWidget() == m_flatCallTreeWidget )
	{	
		QTreeWidget *currentTree = (QTreeWidget *)m_tabWidget->currentWidget();
		if( currentTree->currentItem() != NULL )
		{
			QString fileName = currentTree->currentItem()->text(5);
			QString lineNum = currentTree->currentItem()->text(6);
			QProcess process;
			QString tempString = m_editorArguments;
			tempString.replace("<filename>", fileName);
			tempString.replace("<linenum>", lineNum);
			process.startDetached( m_editorPath, tempString.split(" ") );
		}
	}
}

void VSPTree::setTextEditor()
{
	m_editorPath = QFileDialog::getOpenFileName(0,"Where is your text editor?",m_editorPath,"*.exe");
	if ( m_editorPath.isNull() ) 
	{
		return;
	}

	bool ok;
	m_editorArguments = QInputDialog::getText(this, "Command line arguments", "Seperated by spaces. Use <filename> for filename and <linenum> for line number", QLineEdit::Normal, m_editorArguments, &ok);
}

void VSPTree::closeEvent( QCloseEvent* e )
{
	QSettings settings( m_applicationPath + "/VSPTree.ini", QSettings::IniFormat );
    settings.setValue( "m_lastExe", m_lastExe );
	settings.setValue( "m_lastVSP", m_lastVSP );
	settings.setValue( "m_lastArguments", m_lastArguments );
	settings.setValue( "m_editorPath", m_editorPath );
	settings.setValue( "m_editorArguments", m_editorArguments );
	e->accept();
}

int VSPTree::writeText(QString filePath,QString string){
	QFile fileout(filePath);
	if (!fileout.open(QIODevice::WriteOnly | QIODevice::Text)){
		Log("Error Cannot Open : " + fileout.fileName());
		return 1;
	}
	QTextStream out(&fileout);
	//out.setCodec("UTF-8");
	out << string;
	fileout.close();
	return 0;
}

QDomDocument VSPTree::readXML(QString filepath){
	QDomDocument doc("Option");
	QFile file(filepath);

	if( !file.open( QIODevice::ReadOnly ) ) {
		Log("Unable to Open " + filepath);
		doc.clear();
		return doc;
	}

	if( !doc.setContent( &file ) ) {
		file.close();
		Log(filepath + " is not well formed");
		doc.clear();
		return doc;
	}
	file.close();

	return doc;
}

void VSPTree::workDone()
{	
	QFileInfo fileinfo(m_lastVSP);	

	QString callTreeSummary = fileinfo.absolutePath() + "/" + fileinfo.baseName() + QString("_CallTreeSummary.xml");
	LoadCallTree(callTreeSummary);

	QString functionSummary = fileinfo.absolutePath() + "/" + fileinfo.baseName() + QString("_FunctionSummary.xml");
	LoadFunctionSummary(functionSummary);
	m_tabWidget->setEnabled(true);
	menubar->setEnabled(true);
}

void VSPTreeThread::run()
{
	QFileInfo fileInfo(m_fileName);
	m_process.setWorkingDirectory(fileInfo.absolutePath());
	m_process.start(m_profilerpath + QString("VSPerfReport.exe"), QStringList() << "/summary:all" << "/packsymbols" << "/XML" << m_fileName );
	while( m_process.state() == QProcess::Running )
	{
		if( m_process.waitForReadyRead() )
		{
			log( QString( m_process.readAllStandardOutput() ) );
			log( QString( m_process.readAllStandardError() ) );
		}
		QTest::qSleep( 250 );
	}
}

void VSPTreeThread::log( QString input )
{
	emit output( input );
}
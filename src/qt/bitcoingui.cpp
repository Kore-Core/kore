// Copyright (c) 2011-2014 The Bitcoin developers
// Copyright (c) 2014-2015 The Dash developers
// Copyright (c) 2015-2018 The KORE developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "bitcoingui.h"

#include "bitcoinunits.h"
#include "captchadialog.h"
#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "miner.h" // StakingCoins
#include "networkstyle.h"
#include "notificator.h"
#include "openuridialog.h"
#include "optionsdialog.h"
#include "optionsmodel.h"
#include "rpcconsole.h"
#include "rpcserver.h" // only used to verify if we can start staking
#include "util.h"
#include "utilitydialog.h"

#ifdef ENABLE_WALLET
#include "blockexplorer.h"
#include "walletframe.h"
#include "walletmodel.h"
#endif // ENABLE_WALLET

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "init.h"
#include "ui_interface.h"
#include "util.h"

#include <iostream>
#include <sys/stat.h>

#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopWidget>
#include <QDragEnterEvent>
#include <QIcon>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QProgressDialog>
#include <QSettings>
#include <QStackedWidget>
#include <QStatusBar>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>
#include <QSizePolicy>

#if QT_VERSION < 0x050000
#include <QTextDocument>
#include <QUrl>
#else
#include <QUrlQuery>
#endif

const QString BitcoinGUI::DEFAULT_WALLET = "~Default";

BitcoinGUI::BitcoinGUI(const NetworkStyle* networkStyle, QWidget* parent) : QMainWindow(parent),
	clientModel(0),
	walletFrame(0),
	unitDisplayControl(0),
    labelObfs4Icon(0),
	labelStakingIcon(0),
	labelEncryptionIcon(0),
	labelConnectionsIcon(0),
	labelBlocksIcon(0),
	progressBarLabel(0),
	progressBar(0),
	progressDialog(0),
	appMenuBar(0),
	overviewAction(0),
	historyAction(0),
	quitAction(0),
	sendCoinsAction(0),
	usedSendingAddressesAction(0),
	usedReceivingAddressesAction(0),
	signMessageAction(0),
	verifyMessageAction(0),
	bip38ToolAction(0),
	multisigCreateAction(0),
	multisigSpendAction(0),
	multisigSignAction(0),
	aboutAction(0),
	receiveCoinsAction(0),
	optionsAction(0),
    browserAction(0),
    fIsTorBrowserRunning(false),
	toggleHideAction(0),
	encryptWalletAction(0),
	backupWalletAction(0),
	changePassphraseAction(0),
	aboutQtAction(0),
	openRPCConsoleAction(0),
	openAction(0),
	showHelpMessageAction(0),
	multiSendAction(0),
	trayIcon(0),
	trayIconMenu(0),
	notificator(0),
	rpcConsole(0),
	explorerWindow(0),
	prevBlocks(0),
	spinnerFrame(0),
    unlockWalletAction(0),
    lockWalletAction(0)
{
    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    GUIUtil::restoreWindowGeometry("nWindow", QSize(750, 650), this);
    
    QString windowTitle = tr("Kore") + " - ";
#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET
    if (enableWallet) {
        windowTitle += tr("Wallet");
    } else {
        windowTitle += tr("Node");
    }
    QString userWindowTitle = QString::fromStdString(GetArg("-windowtitle", ""));
    if (!userWindowTitle.isEmpty()) windowTitle += " - " + userWindowTitle;
    windowTitle += " " + networkStyle->getTitleAddText();
#ifndef Q_OS_MAC
    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());
#else
    MacDockIconHandler::instance()->setIcon(networkStyle->getAppIcon());
#endif
    setWindowTitle(windowTitle);

#if defined(Q_OS_MAC) && QT_VERSION < 0x050000
    // This property is not implemented in Qt 5. Setting it has no effect.
    // A replacement API (QtMacUnifiedToolBar) is available in QtMacExtras.
    setUnifiedTitleAndToolBarOnMac(true);
#endif

    rpcConsole = new RPCConsole(enableWallet ? this : 0);
#ifdef ENABLE_WALLET
    if (enableWallet) {
        /** Create wallet frame*/
        walletFrame = new WalletFrame(this);
        explorerWindow = new BlockExplorer(this);
    } else
#endif // ENABLE_WALLET
    {
        /* When compiled without wallet or -disablewallet is provided,
         * the central widget is the rpc console.
         */
        setCentralWidget(rpcConsole);
    }

    // Accept D&D of URIs
    setAcceptDrops(true);

    // Create actions for the toolbar, menu bar and tray/dock icon
    // Needs walletFrame to be initialized
    createActions(networkStyle);

    // Create application menu bar
    createMenuBar();

    // Create the toolbars
    createToolBars();

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Create status bar
    statusBar();

    // Status bar notification icons
    QFrame* frameBlocks = new QFrame();
    frameBlocks->setContentsMargins(0, 0, 0, 0);
    frameBlocks->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QHBoxLayout* frameBlocksLayout = new QHBoxLayout(frameBlocks);
    frameBlocksLayout->setContentsMargins(0, 0, 0, 0);
    frameBlocksLayout->setSpacing(8);
    unitDisplayControl = new UnitDisplayStatusBarControl();
    unitDisplayControl->setObjectName("unitDisplayControl");
    labelObfs4Icon = new Obfs4ControlLabel();
    labelObfs4Icon->setObjectName("labelStakingIcon");
    labelStakingIcon = new StakingStatusBarControl();
    labelStakingIcon->setObjectName("labelStakingIcon");
    labelEncryptionIcon = new QPushButton();
    labelEncryptionIcon->setFixedSize(STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE+3,STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE);
    labelEncryptionIcon->setText("");
    labelEncryptionIcon->setIconSize(QSize(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelEncryptionIcon->setObjectName("labelEncryptionIcon");
    labelEncryptionIcon->setFlat(true); // Make the button look like a label, but clickable
    //labelEncryptionIcon->setMaximumSize(STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE, STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE);
    labelConnectionsIcon = new QPushButton();
    labelConnectionsIcon->setFixedSize(STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE+3,STATUSBAR_ICONSIZE+STATUSBAR_ICON_SELECTION_SHADE);
    labelConnectionsIcon->setText("");
    labelConnectionsIcon->setIconSize(QSize(STATUSBAR_ICONSIZE,STATUSBAR_ICONSIZE));
    labelConnectionsIcon->setObjectName("labelConnectionsIcon");
    labelConnectionsIcon->setFlat(true); // Make the button look like a label, but clickable
    //labelConnectionsIcon->setMaximumSize(STATUSBAR_ICONSIZE + STATUSBAR_ICON_SELECTION_SHADE, STATUSBAR_ICONSIZE + STATUSBAR_ICON_SELECTION_SHADE);
    labelBlocksIcon = new QLabel();
    labelBlocksIcon->setObjectName("labelBlocksIcon");


    if (enableWallet) {
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(unitDisplayControl);
        frameBlocksLayout->addStretch();
        frameBlocksLayout->addWidget(labelEncryptionIcon);
    }
    frameBlocksLayout->addWidget(labelObfs4Icon);
    frameBlocksLayout->addStretch();

    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelStakingIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelConnectionsIcon);
    frameBlocksLayout->addStretch();
    frameBlocksLayout->addWidget(labelBlocksIcon);
    frameBlocksLayout->addStretch();

    // Progress bar and label for blocks download
    progressBarLabel = new QLabel();
    progressBarLabel->setVisible(true);
    progressBarLabel->setObjectName("progressBarLabel");
    progressBar = new GUIUtil::ProgressBar();
    progressBar->setAlignment(Qt::AlignCenter);
    progressBar->setVisible(true);

    // Override style sheet for progress bar for styles that have a segmented progress bar,
    // as they make the text unreadable (workaround for issue #1071)
    // See https://qt-project.org/doc/qt-4.8/gallery.html
    /*
    QString curStyle = QApplication::style()->metaObject()->className();
    if (curStyle == "QWindowsStyle" || curStyle == "QWindowsXPStyle") {
        progressBar->setStyleSheet("QProgressBar { background-color: #F8F8F8; border: 1px solid grey; border-radius: 7px; padding: 1px; text-align: center; } QProgressBar::chunk { background: QLinearGradient(x1: 0, y1: 0, x2: 1, y2: 0, stop: 0 #00CCFF, stop: 1 #33CCFF); border-radius: 7px; margin: 0px; }");
    }
    */

    statusBar()->addWidget(progressBarLabel);
    statusBar()->addWidget(progressBar);
    statusBar()->addPermanentWidget(frameBlocks);

    // the obfs4 can emit a restart signal
    connect(labelObfs4Icon, SIGNAL(handleRestart(QStringList)), this, SLOT(handleRestart(QStringList)));

    // Jump directly to tabs in RPC-console
    connect(openInfoAction, SIGNAL(triggered()), rpcConsole, SLOT(showInfo()));
    connect(openRPCConsoleAction, SIGNAL(triggered()), rpcConsole, SLOT(showConsole()));
    connect(openNetworkAction, SIGNAL(triggered()), rpcConsole, SLOT(showNetwork()));
    connect(openPeersAction, SIGNAL(triggered()), rpcConsole, SLOT(showPeers()));
    connect(openRepairAction, SIGNAL(triggered()), rpcConsole, SLOT(showRepair()));
    connect(openConfEditorAction, SIGNAL(triggered()), rpcConsole, SLOT(showConfEditor()));
    connect(showBackupsAction, SIGNAL(triggered()), rpcConsole, SLOT(showBackups()));
    connect(labelConnectionsIcon, SIGNAL(clicked()), rpcConsole, SLOT(showPeers()));
    connect(labelEncryptionIcon, SIGNAL(clicked()), walletFrame, SLOT(toggleLockWallet()));

    // Get restart command-line parameters and handle restart
    connect(rpcConsole, SIGNAL(handleRestart(QStringList)), this, SLOT(handleRestart(QStringList)));

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), rpcConsole, SLOT(hide()));

    connect(openBlockExplorerAction, SIGNAL(triggered()), explorerWindow, SLOT(show()));

    // prevents an open debug window from becoming stuck/unusable on client shutdown
    connect(quitAction, SIGNAL(triggered()), explorerWindow, SLOT(hide()));

#ifdef ENABLE_TOR_BROWSER
    torBrowserProcess = new QProcess(this);
    connect(torBrowserProcess, SIGNAL(finished(int , QProcess::ExitStatus )), this, SLOT(on_torBrowserProcessExit(int , QProcess::ExitStatus )));
#endif    

    // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    this->installEventFilter(this);

    // Initially wallet actions should be disabled
    setWalletActionsEnabled(false);

    // Subscribe to notifications from core
    subscribeToCoreSignals();

}

BitcoinGUI::~BitcoinGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    delete appMenuBar;
    MacDockIconHandler::cleanup();
#endif
#ifdef ENABLE_TOR_BROWSER
    KillTorBrowserProcess();
    if (torBrowserProcess)
      delete torBrowserProcess;
#endif    
}

void BitcoinGUI::createActions(const NetworkStyle* networkStyle)
{
    QActionGroup* tabGroup = new QActionGroup(this);

    overviewAction = new QAction(QIcon(":/icons/overview"),tr("&MY WALLET"), this);
    overviewAction->setStatusTip(tr("Show general overview of wallet"));
    overviewAction->setToolTip(overviewAction->statusTip());
    overviewAction->setCheckable(true);
    overviewAction->setObjectName("overviewAction");
#ifdef Q_OS_MAC
    overviewAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_1));
#else
    overviewAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_1));
#endif
    tabGroup->addAction(overviewAction);

    sendCoinsAction = new QAction(QIcon(":/icons/send"), tr("&EASY SEND"), this);
    sendCoinsAction->setStatusTip(tr("Send coins to a KORE address"));
    sendCoinsAction->setToolTip(sendCoinsAction->statusTip());
    sendCoinsAction->setCheckable(true);
#ifdef Q_OS_MAC
    sendCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_2));
#else
    sendCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_2));
#endif
    tabGroup->addAction(sendCoinsAction);

    receiveCoinsAction = new QAction(QIcon(":/icons/receiving_addresses"), tr("&EASY RECEIVE"), this);
    receiveCoinsAction->setStatusTip(tr("Request payments (generates QR codes and kore: URIs)"));
    receiveCoinsAction->setToolTip(receiveCoinsAction->statusTip());
    receiveCoinsAction->setCheckable(true);
#ifdef Q_OS_MAC
    receiveCoinsAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_3));
#else
    receiveCoinsAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_3));
#endif
    tabGroup->addAction(receiveCoinsAction);

    historyAction = new QAction(QIcon(":/icons/history"), tr("&TRANSACTIONS"), this);
    historyAction->setStatusTip(tr("Browse your transaction history"));
    historyAction->setToolTip(historyAction->statusTip());
    historyAction->setCheckable(true);

#ifdef Q_OS_MAC
    historyAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_4));
#else
    historyAction->setShortcut(QKeySequence(Qt::ALT + Qt::Key_4));
#endif
    tabGroup->addAction(historyAction);
#ifdef ENABLE_TOR_BROWSER
    browserAction = new QAction(QIcon(":/icons/browser"), tr("&Tor Browser..."), this);
    browserAction->setStatusTip(tr("Surf's up!"));
    browserAction->setToolTip(browserAction->statusTip());
    browserAction->setCheckable(true);
    tabGroup->addAction(browserAction);
#endif    

#ifdef ENABLE_WALLET

    QSettings settings;

    // These showNormalIfMinimized are needed because Send Coins and Receive Coins
    // can be triggered from the tray menu, and need to show the GUI to be useful.
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(overviewAction, SIGNAL(triggered()), this, SLOT(gotoOverviewPage()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(sendCoinsAction, SIGNAL(triggered()), this, SLOT(gotoSendCoinsPage()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(receiveCoinsAction, SIGNAL(triggered()), this, SLOT(gotoReceiveCoinsPage()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(showNormalIfMinimized()));
    connect(historyAction, SIGNAL(triggered()), this, SLOT(gotoHistoryPage()));
#endif // ENABLE_WALLET
#ifdef ENABLE_TOR_BROWSER
    connect(browserAction, SIGNAL(triggered()), this, SLOT(browserClicked()));
#endif 

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);
    aboutAction = new QAction(networkStyle->getAppIcon(), tr("&About Kore"), this);
    aboutAction->setStatusTip(tr("Show information about Kore"));
    aboutAction->setMenuRole(QAction::AboutRole);
#if QT_VERSION < 0x050000
    aboutQtAction = new QAction(QIcon(":/trolltech/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#else
    aboutQtAction = new QAction(QIcon(":/qt-project.org/qmessagebox/images/qtlogo-64.png"), tr("About &Qt"), this);
#endif
    aboutQtAction->setStatusTip(tr("Show information about Qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    optionsAction = new QAction(QIcon(":/icons/options"), tr("&Options..."), this);
    optionsAction->setStatusTip(tr("Modify configuration options for KORE"));
    optionsAction->setMenuRole(QAction::PreferencesRole);
    toggleHideAction = new QAction(networkStyle->getAppIcon(), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    encryptWalletAction = new QAction(QIcon(":/icons/lock_closed"), tr("&Encrypt Wallet..."), this);
    encryptWalletAction->setStatusTip(tr("Encrypt the private keys that belong to your wallet"));
    encryptWalletAction->setCheckable(true);
    backupWalletAction = new QAction(QIcon(":/icons/filesave"), tr("&Backup Wallet..."), this);
    backupWalletAction->setStatusTip(tr("Backup wallet to another location"));
    changePassphraseAction = new QAction(QIcon(":/icons/key"), tr("&Change Passphrase..."), this);
    changePassphraseAction->setStatusTip(tr("Change the passphrase used for wallet encryption"));
    unlockWalletAction = new QAction(tr("&Unlock Wallet..."), this);
    unlockWalletAction->setToolTip(tr("Unlock wallet"));
    lockWalletAction = new QAction(tr("&Lock Wallet"), this);
    signMessageAction = new QAction(QIcon(":/icons/edit"), tr("Sign &message..."), this);
    signMessageAction->setStatusTip(tr("Sign messages with your KORE addresses to prove you own them"));
    verifyMessageAction = new QAction(QIcon(":/icons/transaction_0"), tr("&Verify message..."), this);
    verifyMessageAction->setStatusTip(tr("Verify messages to ensure they were signed with specified KORE addresses"));
    bip38ToolAction = new QAction(QIcon(":/icons/key"), tr("&BIP38 tool"), this);
    bip38ToolAction->setToolTip(tr("Encrypt and decrypt private keys using a passphrase"));
    multiSendAction = new QAction(QIcon(":/icons/edit"), tr("&MultiSend"), this);
    multiSendAction->setToolTip(tr("MultiSend Settings"));
    multiSendAction->setCheckable(true);

    openInfoAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Information"), this);
    openInfoAction->setStatusTip(tr("Show diagnostic information"));
    openRPCConsoleAction = new QAction(QIcon(":/icons/debugwindow"), tr("&Debug console"), this);
    openRPCConsoleAction->setStatusTip(tr("Open debugging console"));
    openNetworkAction = new QAction(QIcon(":/icons/connect_4"), tr("&Network Monitor"), this);
    openNetworkAction->setStatusTip(tr("Show network monitor"));
    openPeersAction = new QAction(QIcon(":/icons/connect_4"), tr("&Peers list"), this);
    openPeersAction->setStatusTip(tr("Show peers info"));
    openRepairAction = new QAction(QIcon(":/icons/options"), tr("Wallet &Repair"), this);
    openRepairAction->setStatusTip(tr("Show wallet repair options"));
    openConfEditorAction = new QAction(QIcon(":/icons/edit"), tr("Open Wallet &Configuration File"), this);
    openConfEditorAction->setStatusTip(tr("Open configuration file"));
    showBackupsAction = new QAction(QIcon(":/icons/browse"), tr("Show Automatic &Backups"), this);
    showBackupsAction->setStatusTip(tr("Show automatically created wallet backups"));

    usedSendingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Sending addresses..."), this);
    usedSendingAddressesAction->setStatusTip(tr("Show the list of used sending addresses and labels"));
    usedReceivingAddressesAction = new QAction(QIcon(":/icons/address-book"), tr("&Receiving addresses..."), this);
    usedReceivingAddressesAction->setStatusTip(tr("Show the list of used receiving addresses and labels"));

    multisigCreateAction = new QAction(QIcon(":/icons/address-book"), tr("&Multisignature creation..."), this);
    multisigCreateAction->setStatusTip(tr("Create a new multisignature address and add it to this wallet"));
    multisigSpendAction = new QAction(QIcon(":/icons/send"), tr("&Multisignature spending..."), this);
    multisigSpendAction->setStatusTip(tr("Spend from a multisignature address"));
    multisigSignAction = new QAction(QIcon(":/icons/editpaste"), tr("&Multisignature signing..."), this);
    multisigSignAction->setStatusTip(tr("Sign with a multisignature address"));

    openAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_FileIcon), tr("Open &URI..."), this);
    openAction->setStatusTip(tr("Open a KORE: URI or payment request"));
    openBlockExplorerAction = new QAction(QIcon(":/icons/explorer"), tr("&Blockchain explorer"), this);
    openBlockExplorerAction->setStatusTip(tr("Block explorer window"));

    showHelpMessageAction = new QAction(QApplication::style()->standardIcon(QStyle::SP_MessageBoxInformation), tr("&Command-line options"), this);
    showHelpMessageAction->setMenuRole(QAction::NoRole);
    showHelpMessageAction->setStatusTip(tr("Show the Kore help message to get a list with possible KORE command-line options"));

    connect(quitAction, SIGNAL(triggered()), qApp, SLOT(quit()));
    connect(aboutAction, SIGNAL(triggered()), this, SLOT(aboutClicked()));
    connect(aboutQtAction, SIGNAL(triggered()), qApp, SLOT(aboutQt()));
    connect(optionsAction, SIGNAL(triggered()), this, SLOT(optionsClicked()));
    connect(toggleHideAction, SIGNAL(triggered()), this, SLOT(toggleHidden()));
    connect(showHelpMessageAction, SIGNAL(triggered()), this, SLOT(showHelpMessageClicked()));
#ifdef ENABLE_WALLET
    if (walletFrame) {
        connect(encryptWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(encryptWallet(bool)));
        connect(backupWalletAction, SIGNAL(triggered()), walletFrame, SLOT(backupWallet()));
        connect(changePassphraseAction, SIGNAL(triggered()), walletFrame, SLOT(changePassphrase()));
		connect(unlockWalletAction, SIGNAL(triggered(bool)), walletFrame, SLOT(unlockWallet(bool)));
        connect(lockWalletAction, SIGNAL(triggered()), walletFrame, SLOT(lockWallet()));
        connect(signMessageAction, SIGNAL(triggered()), this, SLOT(gotoSignMessageTab()));
        connect(verifyMessageAction, SIGNAL(triggered()), this, SLOT(gotoVerifyMessageTab()));
        connect(bip38ToolAction, SIGNAL(triggered()), this, SLOT(gotoBip38Tool()));
        connect(usedSendingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedSendingAddresses()));
        connect(usedReceivingAddressesAction, SIGNAL(triggered()), walletFrame, SLOT(usedReceivingAddresses()));
        connect(openAction, SIGNAL(triggered()), this, SLOT(openClicked()));
        connect(multiSendAction, SIGNAL(triggered()), this, SLOT(gotoMultiSendDialog()));
        connect(multisigCreateAction, SIGNAL(triggered()), this, SLOT(gotoMultisigCreate()));
        connect(multisigSpendAction, SIGNAL(triggered()), this, SLOT(gotoMultisigSpend()));
        connect(multisigSignAction, SIGNAL(triggered()), this, SLOT(gotoMultisigSign()));
    }
#endif // ENABLE_WALLET
}

void BitcoinGUI::createMenuBar()
{
#ifdef Q_OS_MAC
    // Create a decoupled menu bar on Mac which stays even if the window is closed
    appMenuBar = new QMenuBar();
#else
    // Get the main window's menu bar on other platforms
    appMenuBar = menuBar();
#endif

    // Configure the menus
    QMenu* file = appMenuBar->addMenu(tr("&File"));
    if (walletFrame) {
        file->addAction(openAction);
        file->addAction(backupWalletAction);
        file->addAction(signMessageAction);
        file->addAction(verifyMessageAction);
        file->addSeparator();
        file->addAction(usedSendingAddressesAction);
        file->addAction(usedReceivingAddressesAction);
        file->addSeparator();
        file->addAction(multisigCreateAction);
        file->addAction(multisigSpendAction);
        file->addAction(multisigSignAction);
        file->addSeparator();
    }
    file->addAction(quitAction);

    QMenu* settings = appMenuBar->addMenu(tr("&Settings"));
    if (walletFrame) {
        settings->addAction(encryptWalletAction);
        settings->addAction(changePassphraseAction);
        settings->addAction(unlockWalletAction);
        settings->addAction(lockWalletAction);
        settings->addAction(bip38ToolAction);
        settings->addAction(multiSendAction);
        settings->addSeparator();
    }
    settings->addAction(optionsAction);

    if (walletFrame) {
        QMenu* tools = appMenuBar->addMenu(tr("&Tools"));
        tools->addAction(openInfoAction);
        tools->addAction(openRPCConsoleAction);
        tools->addAction(openNetworkAction);
        tools->addAction(openPeersAction);
        tools->addAction(openRepairAction);
        tools->addSeparator();
        tools->addAction(openConfEditorAction);
        tools->addAction(showBackupsAction);
        tools->addAction(openBlockExplorerAction);
    }

    QMenu* help = appMenuBar->addMenu(tr("&Help"));
    help->addAction(showHelpMessageAction);
    help->addSeparator();
    help->addAction(aboutAction);
    help->addAction(aboutQtAction);
}

void BitcoinGUI::createToolBars()
{


    if (walletFrame) {
        QLabel* header = new QLabel();
        //header->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        header->setPixmap(QPixmap(":/images/about"));
        header->setAlignment(Qt::AlignHCenter);
        header->setMaximumSize(148,96);
        header->setScaledContents(true);
        QToolBar* toolbar = new QToolBar(tr("Tabs toolbar"));
        toolbar->setObjectName("Main-Toolbar"); // Name for CSS addressing
        toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        // Add some empty space at the top of the toolbars
        toolbar->setObjectName("ToolbarSpacer");
        toolbar->addWidget(header);
        toolbar->addAction(overviewAction);
        toolbar->addAction(sendCoinsAction);
        toolbar->addAction(receiveCoinsAction);
        toolbar->addAction(historyAction);
#ifdef ENABLE_TOR_BROWSER
        toolbar->addAction(browserAction);
#endif        
        QSettings settings;
        toolbar->setMovable(false); // remove unused icon in upper left corner
        toolbar->setOrientation(Qt::Vertical);
        toolbar->setIconSize(QSize(20,20));
        overviewAction->setChecked(true);

        /** Create additional container for toolbar and walletFrame and make it the central widget.
            This is a workaround mostly for toolbar styling on Mac OS but should work fine for every other OSes too.
        */
        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(toolbar);
        layout->addWidget(walletFrame);
        layout->setSpacing(0);
        layout->setContentsMargins(QMargins());
        layout->setDirection(QBoxLayout::LeftToRight);
        QWidget* containerWidget = new QWidget();
        containerWidget->setLayout(layout);
        setCentralWidget(containerWidget);
    }


}

void BitcoinGUI::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;
    if (clientModel) {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        // Keep up to date with client
        setNumConnections(clientModel->getNumConnections());
        connect(clientModel, SIGNAL(numConnectionsChanged(int)), this, SLOT(setNumConnections(int)));

        setNumBlocks(clientModel->getNumBlocks());
        connect(clientModel, SIGNAL(numBlocksChanged(int)), this, SLOT(setNumBlocks(int)));

        // Receive and report messages from client model
        connect(clientModel, SIGNAL(message(QString, QString, unsigned int)), this, SLOT(message(QString, QString, unsigned int)));

        // Show progress dialog
        connect(clientModel, SIGNAL(showProgress(QString, int)), this, SLOT(showProgress(QString, int)));

        rpcConsole->setClientModel(clientModel);
#ifdef ENABLE_WALLET
        if (walletFrame) {
            walletFrame->setClientModel(clientModel);
        }
#endif // ENABLE_WALLET
        unitDisplayControl->setOptionsModel(clientModel->getOptionsModel());
        labelStakingIcon->setOptionsModel(clientModel->getOptionsModel());
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if (trayIconMenu) {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

#ifdef ENABLE_WALLET
bool BitcoinGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    if (!walletFrame)
        return false;
    setWalletActionsEnabled(true);
    return walletFrame->addWallet(name, walletModel);
}

bool BitcoinGUI::setCurrentWallet(const QString& name)
{
    if (!walletFrame)
        return false;
    return walletFrame->setCurrentWallet(name);
}

void BitcoinGUI::removeAllWallets()
{
    if (!walletFrame)
        return;
    setWalletActionsEnabled(false);
    walletFrame->removeAllWallets();
}
#endif // ENABLE_WALLET

void BitcoinGUI::setWalletActionsEnabled(bool enabled)
{
    overviewAction->setEnabled(enabled);
    sendCoinsAction->setEnabled(enabled);
    receiveCoinsAction->setEnabled(enabled);
    historyAction->setEnabled(enabled);
#ifdef ENABLE_TOR_BROWSER    
    browserAction->setEnabled(enabled);    
#endif    
    encryptWalletAction->setEnabled(enabled);
    backupWalletAction->setEnabled(enabled);
    changePassphraseAction->setEnabled(enabled);
    signMessageAction->setEnabled(enabled);
    verifyMessageAction->setEnabled(enabled);
    multisigCreateAction->setEnabled(enabled);
    multisigSpendAction->setEnabled(enabled);
    multisigSignAction->setEnabled(enabled);
    bip38ToolAction->setEnabled(enabled);
    usedSendingAddressesAction->setEnabled(enabled);
    usedReceivingAddressesAction->setEnabled(enabled);
    openAction->setEnabled(enabled);
}

void BitcoinGUI::createTrayIcon(const NetworkStyle* networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("Kore client") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->show();
#endif

    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

void BitcoinGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-Mac OSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
        this, SLOT(trayIconActivated(QSystemTrayIcon::ActivationReason)));
#else
    // Note: On Mac, the dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    dockIconHandler->setMainWindow((QMainWindow*)this);
    trayIconMenu = dockIconHandler->dockMenu();
#endif

    // Configuration of the tray icon (or dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();
#ifdef ENABLE_TOR_BROWSER
    trayIconMenu->addAction(browserAction);
    trayIconMenu->addSeparator();
#endif    
    trayIconMenu->addAction(sendCoinsAction);
    trayIconMenu->addAction(receiveCoinsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(signMessageAction);
    trayIconMenu->addAction(verifyMessageAction);
    trayIconMenu->addAction(bip38ToolAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(optionsAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openInfoAction);
    trayIconMenu->addAction(openRPCConsoleAction);
    trayIconMenu->addAction(openNetworkAction);
    trayIconMenu->addAction(openPeersAction);
    trayIconMenu->addAction(openRepairAction);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(openConfEditorAction);
    trayIconMenu->addAction(showBackupsAction);
    trayIconMenu->addAction(openBlockExplorerAction);
#ifndef Q_OS_MAC // This is built-in on Mac
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void BitcoinGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#endif

void BitcoinGUI::optionsClicked()
{
    if (!clientModel || !clientModel->getOptionsModel())
        return;

    OptionsDialog dlg(this, enableWallet);
    dlg.setModel(clientModel->getOptionsModel());
    dlg.exec();

    if (dlg.isRestartRequired()) {
        QMessageBox::StandardButton btnRetVal = QMessageBox::Cancel;
        bool noQuestions = dlg.restartNoQuestions();
        if (!noQuestions) {
            btnRetVal = QMessageBox::question(this, tr("Confirm Restart"),
                tr("Client restart required to activate changes.") + "<br><br>" + tr("Do you want to restart now ?"),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        }

        if (noQuestions || btnRetVal == QMessageBox::Yes) {
            // if necessary to restart
            QStringList args = QApplication::arguments();
            args.removeFirst();

            // Remove existing repair-options
            args.removeAll(SALVAGEWALLET);
            args.removeAll(RESCAN);
            args.removeAll(ZAPTXES1);
            args.removeAll(ZAPTXES2);
            args.removeAll(UPGRADEWALLET);
            args.removeAll(REINDEX);

            // Send command-line arguments to BitcoinGUI::handleRestart()
            emit handleRestart(args);

            return;
        }
    }
}

void BitcoinGUI::aboutClicked()
{
    if (!clientModel)
        return;

    HelpMessageDialog dlg(this, true);
    dlg.exec();
}

void BitcoinGUI::showHelpMessageClicked()
{
    HelpMessageDialog* help = new HelpMessageDialog(this, false);
    help->setAttribute(Qt::WA_DeleteOnClose);
    help->show();
}


#ifdef WIN32
#include <stdlib.h>
void setenv( const char *envname, const char *envval, int overwrite)
{
   _putenv_s(envname, envval);
}
#endif

#ifdef ENABLE_TOR_BROWSER
void BitcoinGUI::SetupTorBrowserEnvironment()
{
    setenv("TOR_SOCKS_PORT", std::to_string(TOR_SOCKS_PORT).c_str(), 1);
    setenv("TOR_CONTROL_PORT", std::to_string(TOR_CONTROL_PORT).c_str(), 1);
    setenv("TOR_SKIP_LAUNCH", "1", 1);
    boost::filesystem::path authCookie;
    authCookie = GetDataDir(true) / "tor" / "control_auth_cookie";
    setenv("TOR_CONTROL_COOKIE_AUTH_FILE", authCookie.string().c_str(), 1);
}
#endif

void BitcoinGUI::browserClicked()
{
#ifdef ENABLE_TOR_BROWSER

#ifdef WIN32
    string torBrowserExec = string(getenv("USERPROFILE")) + "\\AppData\\Local\\Kore\\Browser\\firefox.exe";
#else
    string torBrowserExec = "/usr/local/share/kore/Browser/start-tor-browser";
#endif

    if (!fIsTorBrowserRunning) {
        struct stat sb;
        bool foundTorBrowser = false;

        if ((stat(torBrowserExec.c_str(), &sb) == 0 && sb.st_mode & S_IXUSR)) {
            foundTorBrowser = true;
        }

        if (foundTorBrowser) {
            SetupTorBrowserEnvironment();
            torBrowserProcess->start(torBrowserExec.c_str());
            fIsTorBrowserRunning = true;
        } else {
            // warn user to check his environment
            QMessageBox::critical(this, tr("Tor Browser not Found !!!"),
                tr("Could not find start-tor-browser, please check your environment PATH variable."));
        }
    }
#endif
}

void BitcoinGUI::on_torBrowserProcessExit(int exitCode, QProcess::ExitStatus exitStatus)
{
    fIsTorBrowserRunning = false;
}


#ifdef ENABLE_WALLET
void BitcoinGUI::openClicked()
{
    OpenURIDialog dlg(this);
    if (dlg.exec()) {
        emit receivedURI(dlg.getURI());
    }
}

void BitcoinGUI::gotoOverviewPage()
{
    overviewAction->setChecked(true);
    if (walletFrame) walletFrame->gotoOverviewPage();
}

void BitcoinGUI::gotoHistoryPage()
{
    historyAction->setChecked(true);
    if (walletFrame) walletFrame->gotoHistoryPage();
}

void BitcoinGUI::gotoReceiveCoinsPage()
{
    receiveCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoReceiveCoinsPage();
}

void BitcoinGUI::gotoSendCoinsPage(QString addr)
{
    sendCoinsAction->setChecked(true);
    if (walletFrame) walletFrame->gotoSendCoinsPage(addr);
}

void BitcoinGUI::gotoSignMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoSignMessageTab(addr);
}

void BitcoinGUI::gotoVerifyMessageTab(QString addr)
{
    if (walletFrame) walletFrame->gotoVerifyMessageTab(addr);
}

void BitcoinGUI::gotoMultisigCreate()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(0);
}

void BitcoinGUI::gotoMultisigSpend()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(1);
}

void BitcoinGUI::gotoMultisigSign()
{
    if(walletFrame) walletFrame->gotoMultisigDialog(2);
}

void BitcoinGUI::gotoBip38Tool()
{
    if (walletFrame) walletFrame->gotoBip38Tool();
}

void BitcoinGUI::gotoMultiSendDialog()
{
    multiSendAction->setChecked(true);
    if (walletFrame)
        walletFrame->gotoMultiSendDialog();
}
void BitcoinGUI::gotoBlockExplorerPage()
{
    if (walletFrame) walletFrame->gotoBlockExplorerPage();
}

#endif // ENABLE_WALLET

void BitcoinGUI::setNumConnections(int count)
{
    QString icon;
    switch (count) {
    case 0:
        icon = ":/icons/connect_0";
        break;
    case 1:
    case 2:
    case 3:
        icon = ":/icons/connect_1";
        break;
    case 4:
    case 5:
    case 6:
        icon = ":/icons/connect_2";
        break;
    case 7:
    case 8:
    case 9:
        icon = ":/icons/connect_3";
        break;
    default:
        icon = ":/icons/connect_4";
        break;
    }
    QIcon connectionItem = QIcon(icon).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE);
    labelConnectionsIcon->setIcon(connectionItem);
    labelConnectionsIcon->setToolTip(tr("%n active connection(s) to KORE network", "", count));
}

void BitcoinGUI::setNumBlocks(int count)
{
    if (!clientModel)
        return;

    // Prevent orphan statusbar messages (e.g. hover Quit in main menu, wait until chain-sync starts -> garbelled text)
    statusBar()->clearMessage();

    // Acquire current block source
    enum BlockSource blockSource = clientModel->getBlockSource();
    switch (blockSource) {
    case BLOCK_SOURCE_NETWORK:
        progressBarLabel->setText(tr("Synchronizing with network..."));
        break;
    case BLOCK_SOURCE_DISK:
        progressBarLabel->setText(tr("Importing blocks from disk..."));
        break;
    case BLOCK_SOURCE_REINDEX:
        progressBarLabel->setText(tr("Reindexing blocks on disk..."));
        break;
    case BLOCK_SOURCE_NONE:
        // Case: not Importing, not Reindexing and no network connection
        progressBarLabel->setText(tr("No block source available..."));
        break;
    }

    QString tooltip;

    QDateTime lastBlockDate = clientModel->getLastBlockDate();
    QDateTime currentDate = QDateTime::currentDateTime();
    int secs = lastBlockDate.secsTo(currentDate);

    tooltip = tr("Processed %n blocks of transaction history.", "", count);

    // Set icon state: spinning if catching up, tick otherwise
    //    if(secs < 25*60) // 90*60 for bitcoin but we are 4x times faster
    if (!IsInitialBlockDownload()) {
        tooltip = tr("Up to date") + QString(".<br>") + tooltip;
        progressBarLabel->setVisible(false);
        progressBar->setVisible(false);
        labelBlocksIcon->setPixmap(QIcon(":/icons/synced").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));

#ifdef ENABLE_WALLET
            if (walletFrame)
                walletFrame->showOutOfSyncWarning(false);
#endif // ENABLE_WALLET
    } else {
        // Represent time from last generated block in human readable text
        QString timeBehindText;
        const int HOUR_IN_SECONDS = 60 * 60;
        const int DAY_IN_SECONDS = 24 * 60 * 60;
        const int WEEK_IN_SECONDS = 7 * 24 * 60 * 60;
        const int YEAR_IN_SECONDS = 31556952; // Average length of year in Gregorian calendar
        if (secs < 2 * DAY_IN_SECONDS) {
            timeBehindText = tr("%n hour(s)", "", secs / HOUR_IN_SECONDS);
        } else if (secs < 2 * WEEK_IN_SECONDS) {
            timeBehindText = tr("%n day(s)", "", secs / DAY_IN_SECONDS);
        } else if (secs < YEAR_IN_SECONDS) {
            timeBehindText = tr("%n week(s)", "", secs / WEEK_IN_SECONDS);
        } else {
            int years = secs / YEAR_IN_SECONDS;
            int remainder = secs % YEAR_IN_SECONDS;
            timeBehindText = tr("%1 and %2").arg(tr("%n year(s)", "", years)).arg(tr("%n week(s)", "", remainder / WEEK_IN_SECONDS));
        }

        progressBarLabel->setVisible(true);
        progressBar->setFormat(tr("%1 behind. Scanning block %2").arg(timeBehindText).arg(count));
        progressBar->setMaximum(1000000000);
        progressBar->setValue(clientModel->getVerificationProgress()*1000000000);
        progressBar->setVisible(true);

        tooltip = tr("Catching up...") + QString("<br>") + tooltip;
        if (count != prevBlocks) {
            labelBlocksIcon->setPixmap(QIcon(QString(
                                                 ":/movies/spinner-%1")
                                                 .arg(spinnerFrame, 3, 10, QChar('0')))
                                           .pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;

           /* Code to spin a file
            QPixmap pixmap(QIcon(QString(":/movies/spinner-001")).pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
            QTransform rm;
            rm.rotate(spinnerFrame*10);
            pixmap = pixmap.transformed(rm);
            labelBlocksIcon->setPixmap(pixmap);
            spinnerFrame = (spinnerFrame + 1) % SPINNER_FRAMES;
            */

        }
        prevBlocks = count;

#ifdef ENABLE_WALLET
        if (walletFrame)
            walletFrame->showOutOfSyncWarning(true);
#endif // ENABLE_WALLET

        tooltip += QString("<br>");
        tooltip += tr("Last received block was generated %1 ago.").arg(timeBehindText);
        tooltip += QString("<br>");
        tooltip += tr("Transactions after this will not yet be visible.");
    }

    // Don't word-wrap this (fixed-width) tooltip
    tooltip = QString("<nobr>") + tooltip + QString("</nobr>");

    labelBlocksIcon->setToolTip(tooltip);
    progressBarLabel->setToolTip(tooltip);
    progressBar->setToolTip(tooltip);
}

void BitcoinGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
{
    QString strTitle = tr("Kore"); // default title
    // Default to information icon
    int nMBoxIcon = QMessageBox::Information;
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            break;
        }
    }
    // Append title to "KORE - "
    if (!msgType.isEmpty())
        strTitle += " - " + msgType;

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nMBoxIcon = QMessageBox::Critical;
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nMBoxIcon = QMessageBox::Warning;
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        QMessageBox::StandardButton buttons;
        if (!(buttons = (QMessageBox::StandardButton)(style & CClientUIInterface::BTN_MASK)))
            buttons = QMessageBox::Ok;

        showNormalIfMinimized();
        QMessageBox mBox((QMessageBox::Icon)nMBoxIcon, strTitle, message, buttons, this);
        int r = mBox.exec();
        if (ret != NULL)
            *ret = r == QMessageBox::Ok;
    } else
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
}

void BitcoinGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, SLOT(hide()));
                e->ignore();
            }
        }
    }
#endif
}

void BitcoinGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}

#ifdef ENABLE_WALLET
void BitcoinGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address)
{
    // On new transaction, make an info balloon
    message((amount) < 0 ? (pwalletMain->fMultiSendNotify == true ? tr("Sent MultiSend transaction") : tr("Sent transaction")) : tr("Incoming transaction"),
        tr("Date: %1\n"
           "Amount: %2\n"
           "Type: %3\n"
           "Address: %4\n")
            .arg(date)
            .arg(BitcoinUnits::formatWithUnit(unit, amount, true))
            .arg(type)
            .arg(address),
        CClientUIInterface::MSG_INFORMATION);

    pwalletMain->fMultiSendNotify = false;
}
#endif // ENABLE_WALLET

void BitcoinGUI::dragEnterEvent(QDragEnterEvent* event)
{
    // Accept only URIs
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void BitcoinGUI::dropEvent(QDropEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        Q_FOREACH (const QUrl& uri, event->mimeData()->urls()) {
            Q_EMIT receivedURI(uri.toString());
        }
    }
    event->acceptProposedAction();
}

bool BitcoinGUI::eventFilter(QObject* object, QEvent* event)
{
    // Catch status tip events
    if (event->type() == QEvent::StatusTip) {
        // Prevent adding text from setStatusTip(), if we currently use the status bar for displaying other stuff
        if (progressBarLabel->isVisible() || progressBar->isVisible())
            return true;
    }
    return QMainWindow::eventFilter(object, event);
}


#ifdef ENABLE_WALLET
bool BitcoinGUI::handlePaymentRequest(const SendCoinsRecipient& recipient)
{
    // URI has to be valid
    if (walletFrame && walletFrame->handlePaymentRequest(recipient)) {
        showNormalIfMinimized();
        gotoSendCoinsPage();
        return true;
    }
    return false;
}

void BitcoinGUI::setEncryptionStatus(int status)
{
    switch (status) {
    case WalletModel::Unencrypted:
        labelEncryptionIcon->hide();
        encryptWalletAction->setChecked(false);
        changePassphraseAction->setEnabled(false);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(true);
        break;
    case WalletModel::Unlocked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(false);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        // Let's show how this affets staking
        labelStakingIcon->setEnabled(true);
        labelStakingIcon->checkStakingTimer();
        
        break;
    case WalletModel::UnlockedForAnonymizationOnly:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(":/icons/lock_open").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>unlocked</b> for staking only"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(true);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        // Let's show how this affets staking
        labelStakingIcon->setEnabled(true);
        labelStakingIcon->checkStakingTimer();

        break;
    case WalletModel::Locked:
        labelEncryptionIcon->show();
        labelEncryptionIcon->setIcon(QIcon(":/icons/lock_closed").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        labelEncryptionIcon->setToolTip(tr("Wallet is <b>encrypted</b> and currently <b>locked</b>"));
        encryptWalletAction->setChecked(true);
        changePassphraseAction->setEnabled(true);
        unlockWalletAction->setVisible(true);
        lockWalletAction->setVisible(false);
        encryptWalletAction->setEnabled(false); // TODO: decrypt currently not supported
        // Let's show how this affets staking
        labelStakingIcon->setEnabled(false);
        labelStakingIcon->checkStakingTimer();

        break;
    }
}
#endif // ENABLE_WALLET

void BitcoinGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if (!clientModel)
        return;

    // activateWindow() (sometimes) helps with keyboard focus on Windows
    if (isHidden()) {
        show();
        activateWindow();
    } else if (isMinimized()) {
        showNormal();
        activateWindow();
    } else if (GUIUtil::isObscured(this)) {
        raise();
        activateWindow();
    } else if (fToggleHidden)
        hide();
}

void BitcoinGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void BitcoinGUI::detectShutdown()
{
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void BitcoinGUI::showProgress(const QString& title, int nProgress)
{
    if (nProgress == 0) {
        progressDialog = new QProgressDialog(title, "", 0, 100);
        progressDialog->setWindowModality(Qt::ApplicationModal);
        progressDialog->setMinimumDuration(0);
        progressDialog->setCancelButton(0);
        progressDialog->setAutoClose(false);
        progressDialog->setValue(0);
    } else if (nProgress == 100) {
        if (progressDialog) {
            progressDialog->close();
            progressDialog->deleteLater();
        }
    } else if (progressDialog)
        progressDialog->setValue(nProgress);
}

static bool ThreadSafeMessageBox(BitcoinGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
        modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(caption)),
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(unsigned int, style),
        Q_ARG(bool*, &ret));
    return ret;
}

void BitcoinGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void BitcoinGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void BitcoinGUI::KillTorBrowserProcess()
{
    if (fIsTorBrowserRunning) {
#ifndef WIN32        
        int pId = torBrowserProcess->pid();

        QProcess get_child_a;
        QStringList get_child_a_cmd;
        get_child_a_cmd << "--ppid" << QString::number(pId) << "-o pid --no-heading";
        get_child_a.start("ps", get_child_a_cmd);
        get_child_a.waitForFinished(1000);
        QString child_a_str = get_child_a.readAllStandardOutput();
        int child_a = child_a_str.toInt();

        QProcess::execute("kill " + QString::number(child_a));
#endif        

        torBrowserProcess->kill();
        fIsTorBrowserRunning = false;
    }
}

/** Get restart command-line parameters and request restart */
void BitcoinGUI::handleRestart(QStringList args)
{
    KillTorBrowserProcess();

    if (!ShutdownRequested())
        emit requestedRestart(args);
}

UnitDisplayStatusBarControl::UnitDisplayStatusBarControl() : optionsModel(0),
                                                             menu(0)
{
    createContextMenu();
    setToolTip(tr("Unit to show amounts in. Click to select another unit."));
}

/** So that it responds to button clicks */
void UnitDisplayStatusBarControl::mousePressEvent(QMouseEvent* event)
{
    onDisplayUnitsClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void UnitDisplayStatusBarControl::createContextMenu()
{
    menu = new QMenu();
    QString  menuStyle(
           "QMenu::item:selected{"
           "background-color: rgba(222, 222, 222);"
           "color: #333;"
           "}"
        );

    menu->setStyleSheet(menuStyle);
    // doesn work reading from default.css ...
    //menu->setStyleSheet(GUIUtil::loadStyleSheet());
    //menu->setObjectName("unitMenu");

    Q_FOREACH (BitcoinUnits::Unit u, BitcoinUnits::availableUnits()) {
        QAction* menuAction = new QAction(QString(BitcoinUnits::name(u)), this);
        menuAction->setData(QVariant(u));
        menu->addAction(menuAction);
    }
    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void UnitDisplayStatusBarControl::setOptionsModel(OptionsModel* optionsModel)
{
    if (optionsModel) {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit(int)));

        // initialize the display units label with the current value in the model.
        updateDisplayUnit(optionsModel->getDisplayUnit());
    }
}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void UnitDisplayStatusBarControl::updateDisplayUnit(int newUnits)
{
    setPixmap(QIcon(":/icons/unit_" + BitcoinUnits::id(newUnits)).pixmap(50, STATUSBAR_ICONSIZE));
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void UnitDisplayStatusBarControl::onDisplayUnitsClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void UnitDisplayStatusBarControl::onMenuSelection(QAction* action)
{
    if (action) {
        optionsModel->setDisplayUnit(action->data());
    }
}

StakingStatusBarControl::StakingStatusBarControl() : optionsModel(0), menu(0)
{
  createContextMenu();

  QTimer* timerStakingIcon = new QTimer(this);
  connect(timerStakingIcon, SIGNAL(timeout()), this, SLOT(checkStakingTimer()));
  timerStakingIcon->start(10000);
}

void StakingStatusBarControl::mousePressEvent(QMouseEvent* event)
{
    onStakingIconClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void StakingStatusBarControl::createContextMenu()
{
    menu = new QMenu();
    QString  menuStyle(
           "QMenu::item:selected{"
           "background-color: rgba(222, 222, 222);"
           "color: #333;"
           "}"
        );

    menu->setStyleSheet(menuStyle);
    // doesn work reading from default.css ...
    //menu->setStyleSheet(GUIUtil::loadStyleSheet());
    //menu->setObjectName("unitMenu");

    // Create Enable Option
    enableStaking = new QAction(QString("Enable Staking"), this);
    enableStaking->setData(QVariant(true));   
    menu->addAction(enableStaking);    

    // Create Disable Option
    disableStaking = new QAction(QString("Disable Staking"), this);
    disableStaking->setData(QVariant(false));
    menu->addAction(disableStaking);

    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onMenuSelection(QAction*)));
}

/** Lets the control know about the Options Model (and its signals) */
void StakingStatusBarControl::setOptionsModel(OptionsModel* optionsModel)
{
    if (optionsModel) {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel, SIGNAL(displayStakingIconChanged(bool)), this, SLOT(updateStakingIcon(bool)));

        // initialize the display units label with the current value in the model.
        bool staking = optionsModel->getStaking();
        enableStaking->setEnabled(staking);
        disableStaking->setEnabled(!staking);
        //StakingCoins(staking);
        updateStakingIcon(staking, false);
    }
}

void StakingStatusBarControl::checkStakingTimer()
{
    std::string statusmessage;
    
    // when rpc is ready we are ready as well !
    // we can start staking untill we have finished starting the application 
    if (RPCIsInWarmup(&statusmessage))
      return;
    // this timer is checking if the staking was activated by the console (rpc)
    bool isStaking = GetBoolArg("-staking", false);
    // here the action was already done by the rpc, so we just need to 
    // refresh the screen
    updateStakingIcon(isStaking, false);
}

void StakingStatusBarControl::updateStakingIcon(bool staking, bool action)
{
    bool fMultiSend = false;

    if (pwalletMain) {
        fMultiSend = pwalletMain->isMultiSendEnabled();
    }

    if (staking) {
        setPixmap(QIcon(":/icons/staking_active").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
        setToolTip(tr("Staking is active%1\n MultiSend: %2").arg( pwalletMain->IsLocked() ? tr(", when unlocked") : tr(" ")).arg(fMultiSend ? tr("Active") : tr("Not Active")));
        if (action && enableStaking->isEnabled() == true) {
            UpdateConfigFileKeyBool("staking", true);
            StakingCoins(true);
        }
        enableStaking->setEnabled(false);
        disableStaking->setEnabled(true);
    } else {
        setPixmap(QIcon(":/icons/staking_inactive").pixmap(STATUSBAR_ICONSIZE + STATUSBAR_ICON_SELECTION_SHADE, STATUSBAR_ICONSIZE));
        setToolTip(tr("Staking is not active\n MultiSend: %1").arg(fMultiSend ? tr("Active") : tr("Not Active")));
        if (action && disableStaking->isEnabled() == true) {
            UpdateConfigFileKeyBool("staking",false);
            StakingCoins(false);
        }
        enableStaking->setEnabled(true);
        disableStaking->setEnabled(false);                    
    }

}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void StakingStatusBarControl::updateStakingIcon(bool staking)
{
     updateStakingIcon(staking, true);
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void StakingStatusBarControl::onStakingIconClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void StakingStatusBarControl::onMenuSelection(QAction* action)
{
    if (action) {
        optionsModel->setStaking(action->data());
    }
}

Obfs4ControlLabel::Obfs4ControlLabel() : optionsModel(0), menu(0)
{
  createContextMenu();
}

void Obfs4ControlLabel::mousePressEvent(QMouseEvent* event)
{
    onObfs4IconClicked(event->pos());
}

/** Creates context menu, its actions, and wires up all the relevant signals for mouse events. */
void Obfs4ControlLabel::createContextMenu()
{
    menu = new QMenu();
    QString  menuStyle(
           "QMenu::item:selected{"
           "background-color: rgba(222, 222, 222);"
           "color: #333;"
           "}"
        );

    menu->setStyleSheet(menuStyle);
    // doesn work reading from default.css ...
    //menu->setStyleSheet(GUIUtil::loadStyleSheet());
    //menu->setObjectName("unitMenu");

    // Create Enable Option
    enableStaking = new QAction(QString("Enable Obfs4"), this);
    enableStaking->setData(QVariant(true));   
    menu->addAction(enableStaking);    

    // Create Disable Option
    disableStaking = new QAction(QString("Disable Obfs4"), this);
    disableStaking->setData(QVariant(false));
    menu->addAction(disableStaking);

    // Create Retrieve Option
    retrieveBridges = new QAction(QString("Update Bridges"), this);
    retrieveBridges->setData(QVariant(false));
    menu->addAction(retrieveBridges);

    //connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(onMenuSelection(QAction*)));

    // lets connect the menu items
    //connect(enableStaking, SIGNAL(triggered(QAction*)), this, SLOT(enablingObfs4(QAction*)));
    connect(enableStaking, &QAction::triggered, this, &Obfs4ControlLabel::enablingObfs4);
    connect(disableStaking, &QAction::triggered, this, &Obfs4ControlLabel::disablingObfs4);
    connect(retrieveBridges, &QAction::triggered, this, &Obfs4ControlLabel::retrievingBridges);

    // initialize the display units label with the current value in the model.
    bool obfs4 = GetBoolArg("-obfs4", false);
    enableStaking->setEnabled(!obfs4);
    disableStaking->setEnabled(obfs4);
    // retrieveBridge will only be enabled if obfs4 is enabled
    retrieveBridges->setEnabled(obfs4);
    if (obfs4)
        setActiveIcon();
    else
        setInactiveIcon();
}

void Obfs4ControlLabel::setActiveIcon()
{
    setPixmap(QIcon(":/icons/obfs4_active").pixmap(STATUSBAR_ICONSIZE, STATUSBAR_ICONSIZE));
    setToolTip(tr("Obfs4 enabled"));
}

void Obfs4ControlLabel::setInactiveIcon()
{
    setPixmap(QIcon(":/icons/obfs4_inactive").pixmap(STATUSBAR_ICONSIZE + STATUSBAR_ICON_SELECTION_SHADE, STATUSBAR_ICONSIZE));
    setToolTip(tr("Obfs4 is not enabled"));
}


/** Lets the control know about the Options Model (and its signals) */
void Obfs4ControlLabel::setOptionsModel(OptionsModel* optionsModel)
{
    if (optionsModel) {
        this->optionsModel = optionsModel;

        // be aware of a display unit change reported by the OptionsModel object.
        connect(optionsModel, SIGNAL(displayObfs4IconChanged(bool)), this, SLOT(updateStakingIcon(bool)));

        // initialize the display units label with the current value in the model.
        bool obfs4 = optionsModel->getObfs4();
        enableStaking->setEnabled(!obfs4);
        disableStaking->setEnabled(obfs4);
        // retrieveBridge will only be enabled if obfs4 is enabled
        retrieveBridges->setEnabled(obfs4);
        if (obfs4)
            setActiveIcon();
        else
            setInactiveIcon();
    }
}

void Obfs4ControlLabel::requestRestart()
{
    QMessageBox::StandardButton btnRetVal = QMessageBox::Cancel;
    btnRetVal = QMessageBox::question(this, tr("Confirm Restart"),
        tr("Client restart required to activate changes.") + "<br><br>" + tr("Do you want to restart now ?"),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if (btnRetVal == QMessageBox::Yes) {
        // if necessary to restart
        QStringList args = QApplication::arguments();
        args.removeFirst();

        // Remove existing repair-options
        args.removeAll(SALVAGEWALLET);
        args.removeAll(RESCAN);
        args.removeAll(ZAPTXES1);
        args.removeAll(ZAPTXES2);
        args.removeAll(UPGRADEWALLET);
        args.removeAll(REINDEX);

        emit handleRestart(args);

        return;
    }
}

void Obfs4ControlLabel::updateStakingIcon(bool staking, bool action)
{
    bool fMultiSend = false;

    if (pwalletMain) {
        fMultiSend = pwalletMain->isMultiSendEnabled();
    }

    if (staking) {
        CaptchaDialog dlg(this);
        int code = dlg.exec();
        if (code == QDialog::Accepted) {
            const QStringList& bridges = dlg.getBridges();
            if (bridges.size() > 0) {
                 QByteArray torrcWithoutBridges;
                if (bridges != GUIUtil::retrieveBridgesFromTorrc(torrcWithoutBridges)) {
                    // save the bridges and ask to restart
                    UpdateConfigFileKeyBool( "obfs4", true );
                    GUIUtil::saveBridges2Torrc(torrcWithoutBridges, bridges);
                    requestRestart();
                } else {
                    // torproject is still returning the same bridges
                    QMessageBox::warning(this, windowTitle(), tr("Tor has returned the same bridges yet, please wait more time, before changing the bridges."),
                        QMessageBox::Ok, QMessageBox::Ok);
                }
            }
        }
    } else {
        if (action && disableStaking->isEnabled() == true) {
            setInactiveIcon();
            UpdateConfigFileKeyBool("obfs4",false);
            requestRestart();
        }
        enableStaking->setEnabled(true);
        disableStaking->setEnabled(false);                    
    }

}

/** When Display Units are changed on OptionsModel it will refresh the display text of the control on the status bar */
void Obfs4ControlLabel::updateStakingIcon(bool staking)
{
     updateStakingIcon(staking, true);
}

/** Shows context menu with Display Unit options by the mouse coordinates */
void Obfs4ControlLabel::onObfs4IconClicked(const QPoint& point)
{
    QPoint globalPos = mapToGlobal(point);
    menu->exec(globalPos);
}

/** Tells underlying optionsModel to update its current display unit. */
void Obfs4ControlLabel::onMenuSelection(QAction* action)
{
    if (action) {
        optionsModel->setStaking(action->data());
    }
}

void Obfs4ControlLabel::solveCaptcha(bool atLeastEnable)
{
    CaptchaDialog dlg(this);
    int code = dlg.exec();
    if (code == QDialog::Accepted) {
        const QStringList& bridges = dlg.getBridges();
        if (bridges.size() > 0) {
            QByteArray torrcWithoutBridges;
            bool differentBridges = bridges != GUIUtil::retrieveBridgesFromTorrc(torrcWithoutBridges);
            if (differentBridges || atLeastEnable) {
                // save the bridges and ask to restart
                UpdateConfigFileKeyBool("obfs4", true);
                if (differentBridges)
                    GUIUtil::saveBridges2Torrc(torrcWithoutBridges, bridges);
                requestRestart();
            } else {
                // torproject is still returning the same bridges
                QMessageBox::warning(this, windowTitle(), tr("Tor has returned the same bridges yet, please wait more time, before changing the bridges."),
                    QMessageBox::Ok, QMessageBox::Ok);
            }
        }
    }
}

void Obfs4ControlLabel::enablingObfs4()
{
    // if we have bridges in the torrc file and are the same, it is still ok
    // to just enable obfs4 in the config file
    solveCaptcha(true);
}

void Obfs4ControlLabel::disablingObfs4()
{    
    UpdateConfigFileKeyBool("obfs4", false);
    requestRestart();
}

void Obfs4ControlLabel::retrievingBridges()
{
    // the bridges needs to be the same
    solveCaptcha(false);
}

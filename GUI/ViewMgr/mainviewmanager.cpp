#include "mainviewmanager.h"
#include "View/mainview.h"
#include "ViewMgr/cbcregviewmanager.h"
#include "ViewMgr/hybridtestviewmanager.h"
#include "ViewMgr/setuptabviewmanager.h"
#include "ViewMgr/calibrateviewmanager.h"

#include <QDebug>

namespace GUI
{
    MainViewManager::MainViewManager(QObject *parent,
                                     MainView &mainView,
                                     SetupTabViewManager &setupVm,
                                     CbcRegViewManager &cbcVm,
                                     HybridTestViewManager &hybridVm,
                                     CalibrateViewManager &calibVm) :
        QObject(parent),
        m_mainView(mainView),
        m_setupVm(setupVm),
        m_cbcRegVm(cbcVm),
        m_hybridTestVm(hybridVm),
        m_calibrateVm(calibVm)
    {
        WireSetupVmMessages();
    }

    MainViewManager::~MainViewManager()
    {
        qDebug() << "Destructing " << this;
    }

    void MainViewManager::WireSetupVmMessages()
    {
        connect(&m_setupVm, SIGNAL(enableAlltabs(bool)),
                &m_mainView, SLOT(enableAllTabsSlot(bool)));

        connect(&m_setupVm, SIGNAL(on2CbcToggle(bool)),
                &m_hybridTestVm, SIGNAL(on2CbcToggle(bool)));

        connect(&m_setupVm, SIGNAL(on2CbcToggle(bool)),
                &m_cbcRegVm, SIGNAL(on2CbcToggle(bool)));
        connect(&m_setupVm, SIGNAL(sendInitialiseRegistersView()),
                &m_cbcRegVm, SIGNAL(sendInitialiseRegistersView()));

        connect(&m_calibrateVm, SIGNAL(startedCalibration()),
                &m_hybridTestVm, SIGNAL(disableLaunch()));
        connect(&m_calibrateVm, SIGNAL(finishedCalibration()),
                &m_hybridTestVm, SIGNAL(enableLaunch()));
    }
}

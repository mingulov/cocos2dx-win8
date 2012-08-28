#include "pch.h"
#include "..\AppDelegate.h"
#include "CCCommon.h"

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
//using namespace Platform;
USING_NS_CC;

namespace cocos2d
{
    // implement in CCApplication_win8_metro.cpp
    extern Windows::ApplicationModel::Core::IFrameworkView^ getSharedCCApplicationFrameworkView();
}

ref class CCApplicationFrameworkViewSource sealed : Windows::ApplicationModel::Core::IFrameworkViewSource 
{
public:
	virtual Windows::ApplicationModel::Core::IFrameworkView^ CreateView()
    {
		return cocos2d::getSharedCCApplicationFrameworkView();
    }
};

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	AppDelegate app;
	auto frameworkViewSource = ref new CCApplicationFrameworkViewSource();
	CoreApplication::Run(frameworkViewSource);

	return 0;
}

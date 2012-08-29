#include "pch.h"

using namespace Windows::ApplicationModel;
using namespace Windows::ApplicationModel::Core;
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

#include "..\AppDelegate.h"
#include "CCCommon.h"
//using namespace Platform;
USING_NS_CC;

[Platform::MTAThread]
int main(Platform::Array<Platform::String^>^)
{
	AppDelegate app;
	auto frameworkViewSource = ref new CCApplicationFrameworkViewSource();
	CoreApplication::Run(frameworkViewSource);

	//auto direct3DApplicationSource = ref new Direct3DApplicationSource();
    //CoreApplication::Run(direct3DApplicationSource);
    return 0;
}

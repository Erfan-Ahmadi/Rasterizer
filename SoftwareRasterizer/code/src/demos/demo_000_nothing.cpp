/*
- DEMO_000
- NOTHING

This Demo is just inheriting from DemoFramework and is nothing other than an empty window we don't render to.
*/

#include "../demo_framework.hpp"

class Demo_000_Nothing : public Demo {
protected:
    virtual bool DoInitResources() override { return true; }
    virtual bool DoExitResources() override { return true; }
    virtual void OnRender() override {}
    virtual void OnUpdate() override {}
};

static auto _ = Demo_Register("Nothing", [] { return new Demo_000_Nothing(); });
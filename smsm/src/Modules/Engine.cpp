#include "Engine.hpp"

#include "Interface.hpp"
#include "Offsets.hpp"
#include "Utils.hpp"
#include "Variable.hpp"
#include "Console.hpp"

#include "SMSM.hpp"

REDECL(Engine::TraceRay);
DETOUR(Engine::TraceRay, const Ray_t& ray, unsigned int fMask, ITraceFilter* pTraceFilter, CGameTrace* pTrace) {
    float requestResult = 0;
    if (smsm.ProcessScriptRequest(ray.m_Start.x, (int)ray.m_Start.y, ray.m_Start.z, &requestResult)) {
        pTrace->fraction = requestResult;
        pTrace->fractionleftsolid = -requestResult + 1;
        //console->Print("nice >:] %f %f\n");
        return 0;
    }
    else {
        auto result = Engine::TraceRay(thisptr, ray, fMask, pTraceFilter, pTrace);
        return result;
    }
}


Variable sv_cheats;

Engine::Engine()
    : Module()
{
}
bool Engine::Init()
{
    if (auto engine = Interface::Create(this->Name(), "VEngineClient0", false)) {
        this->GetActiveSplitScreenPlayerSlot = engine->Original<_GetActiveSplitScreenPlayerSlot>(Offsets::GetActiveSplitScreenPlayerSlot);

        typedef void* (*_GetClientState)();
        auto ClientCmd = engine->Original(Offsets::ClientCmd);
        auto GetClientState = Memory::Read<_GetClientState>(ClientCmd + Offsets::GetClientStateFunction);
        auto cl = Interface::Create(GetClientState(), false);
        if (cl) {
            auto SetSignonState = cl->Original(Offsets::SetSignonState);
            auto HostState_OnClientConnected = Memory::Read(SetSignonState + Offsets::HostState_OnClientConnected);
            this->hoststate = Memory::Deref<CHostState*>(HostState_OnClientConnected + Offsets::hoststate);
        }

        Memory::Read<_Cbuf_AddText>(ClientCmd + Offsets::Cbuf_AddText, &this->Cbuf_AddText);
        Memory::Deref<void*>((uintptr_t)this->Cbuf_AddText + Offsets::s_CommandBuffer, &this->s_CommandBuffer);

        if (this->s_CommandBuffer) {
            auto m_bWaitEnabled = reinterpret_cast<bool*>((uintptr_t)s_CommandBuffer + Offsets::m_bWaitEnabled);
            auto m_bWaitEnabled2 = reinterpret_cast<bool*>((uintptr_t)m_bWaitEnabled + Offsets::CCommandBufferSize);
            *m_bWaitEnabled = *m_bWaitEnabled2 = true;
        }

        this->engineTrace = Interface::Create(this->Name(), "EngineTraceServer004");
        if (this->engineTrace) {
            this->engineTrace->Hook(Engine::TraceRay_Hook, Engine::TraceRay, Offsets::TraceRay);
        }
    }

    if (auto g_VEngineServer = Interface::Create(this->Name(), "VEngineServer0", false)) {
        this->ClientCommand = g_VEngineServer->Original<_ClientCommand>(Offsets::ClientCommand);
    }

    sv_cheats = Variable("sv_cheats");

    return this->hasLoaded = this->hoststate
        && this->engineTrace
        && this->GetActiveSplitScreenPlayerSlot
        && this->Cbuf_AddText
        && this->s_CommandBuffer
        && this->ClientCommand
        && !!sv_cheats;
}

void Engine::Shutdown()
{
    if (this->s_CommandBuffer) {
        auto m_bWaitEnabled = reinterpret_cast<bool*>((uintptr_t)s_CommandBuffer + Offsets::m_bWaitEnabled);
        auto m_bWaitEnabled2 = reinterpret_cast<bool*>((uintptr_t)m_bWaitEnabled + Offsets::CCommandBufferSize);
        *m_bWaitEnabled = *m_bWaitEnabled2 = false;
    }
    if (this->engineTrace) {
        Interface::Delete(this->engineTrace);
    }
}

Engine* engine;

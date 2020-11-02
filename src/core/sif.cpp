#include <cstdio>
#include <cstring>
#include "sif.hpp"

#include "iop/iop_dma.hpp"
#include "ee/dmac.hpp"
#include "ee/emotion.hpp"

void default_rpc(SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
{
    printf("[SIFRPC] Unknown function $%08X called on %s ($%08X)\n", fno, server.name.c_str(), server.module_id);
}

SubsystemInterface::SubsystemInterface(EmotionEngine* ee, IOP_DMA* iop_dma, DMAC* dmac) :
    ee(ee), iop_dma(iop_dma), dmac(dmac)
{

}

void SubsystemInterface::reset()
{
    std::queue<uint32_t> empty, empty2;
    SIF0_FIFO.swap(empty);
    SIF1_FIFO.swap(empty2);
    mscom = 0;
    smcom = 0;
    msflag = 0;
    smflag = 0;
    control = 0;

    rpc_servers.clear();
    register_system_servers();
}

void SubsystemInterface::register_system_servers()
{
    auto default_rpc_lambda = [] (SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
    {
        default_rpc(server, fno, buffer, size);
    };

    auto iopheap_lambda = [=] (SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
    {
        switch (fno)
        {
            case 0x1:
                //sceSifAllocIopHeap
                printf("[SIFRPC] sceSifAllocIopHeap($%08X)\n", ee->read32(buffer));
                break;
            case 0x2:
                //sceSifFreeIopHeap
                printf("[SIFRPC] sceSifFreeIopHeap($%08X)\n", ee->read32(buffer));
                break;
            default:
                default_rpc(server, fno, buffer, size);
        }
    };

    auto cdncmd_lambda = [=] (SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
    {
        switch (fno)
        {
            case 0x1:
                //sceCdRead
            {
                uint32_t lbn = ee->read32(buffer);
                uint32_t sectors = ee->read32(buffer + 4);
                uint32_t data = ee->read32(buffer + 8);
                printf("[SIFRPC] sceCdRead(%d, %d, $%08X)\n", lbn, sectors, data);
            }
                break;
            default:
                default_rpc(server, fno, buffer, size);
        }
    };

    auto cdsearchfile_lambda = [=] (SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
    {
        if (fno == 0)
        {
            uint32_t ptr = buffer + 0x24;
            std::string name;
            while (ee->read8(ptr) && name.size() < 0x100)
            {
                name += ee->read8(ptr);
                ptr++;
            }
            printf("[SIFRPC] sceCdSearchFile(\"%s\")\n", name.c_str());
        }
        else
            default_rpc(server, fno, buffer, size);
    };

    sifrpc_register_server("FILEIO", 0x80000001, default_rpc_lambda);
    sifrpc_register_server("IOPHEAP", 0x80000003, iopheap_lambda);
    sifrpc_register_server("LOADFILE", 0x80000006, default_rpc_lambda);
    sifrpc_register_server("PAD1", 0x80000100, default_rpc_lambda);
    sifrpc_register_server("PAD2", 0x80000101, default_rpc_lambda);
    sifrpc_register_server("MCMAN", 0x80000400, default_rpc_lambda);
    sifrpc_register_server("CDINIT", 0x80000592, default_rpc_lambda);
    sifrpc_register_server("CDSCMD", 0x80000593, default_rpc_lambda);
    sifrpc_register_server("CDNCMD", 0x80000595, cdncmd_lambda);
    sifrpc_register_server("CDSEARCHFILE", 0x80000597, cdsearchfile_lambda);
    sifrpc_register_server("CDDISKREADY", 0x8000059A, default_rpc_lambda);
    sifrpc_register_server("SDREMOTE", 0x80000701, default_rpc_lambda);
}

void SubsystemInterface::sifrpc_register_server(std::string name, uint32_t module_id,
                                                std::function<void (SifRpcServer &, uint32_t, uint32_t, uint32_t)>
                                                func)
{
    SifRpcServer server;
    server.name = name;
    server.module_id = module_id;
    server.client_ptr = 0;
    server.rpc_func = func;
    rpc_servers.push_back(server);
}

bool SubsystemInterface::sifrpc_bind(uint32_t module, uint32_t client)
{
    for (auto it = rpc_servers.begin(); it != rpc_servers.end(); it++)
    {
        if (it->module_id == module)
        {
            //Server found
            printf("[SIFRPC] Client $%08X binding to %s ($%08X)\n", client, it->name.c_str(), module);
            if (!it->client_ptr)
                it->client_ptr = client;
            return true;
        }
    }
    //No server found, we will need to create a new one
    return false;
}

void SubsystemInterface::write_SIF0(uint32_t word)
{
    if (SIF0_FIFO.size() < 4)
        oldest_SIF0_data[SIF0_FIFO.size()] = word;
    SIF0_FIFO.push(word);
    if (SIF0_FIFO.size() >= MAX_FIFO_SIZE)
        iop_dma->clear_DMA_request(IOP_SIF0);
    if (SIF0_FIFO.size() >= 4)
        dmac->set_DMA_request(EE_SIF0);
}

void SubsystemInterface::send_SIF0_junk(int count)
{
    uint32_t temp[4];
    memcpy(temp, oldest_SIF0_data, 4);
    for (int i = 4 - count; i < 4; i++)
    {
        printf("[SIF] Send junk: $%08X\n", temp[i]);
        write_SIF0(temp[i]);
    }
}

void SubsystemInterface::write_SIF1(uint128_t quad)
{
    //printf("[SIF] Write SIF1: $%08X_%08X_%08X_%08X\n", quad._u32[3], quad._u32[2], quad._u32[1], quad._u32[0]);
    for (int i = 0; i < 4; i++)
        SIF1_FIFO.push(quad._u32[i]);
    iop_dma->set_DMA_request(IOP_SIF1);
    if (SIF1_FIFO.size() >= MAX_FIFO_SIZE / 2)
        dmac->clear_DMA_request(EE_SIF1);
}

uint32_t SubsystemInterface::read_SIF0()
{
    uint32_t value = SIF0_FIFO.front();
    SIF0_FIFO.pop();
    iop_dma->set_DMA_request(IOP_SIF0);

    if (SIF0_FIFO.size() < 4)
        dmac->clear_DMA_request(EE_SIF0);
    return value;
}

uint32_t SubsystemInterface::read_SIF1()
{
    uint32_t value = SIF1_FIFO.front();
    SIF1_FIFO.pop();
    if (!SIF1_FIFO.size())
        iop_dma->clear_DMA_request(IOP_SIF1);
    if (SIF1_FIFO.size() < MAX_FIFO_SIZE / 2)
        dmac->set_DMA_request(EE_SIF1);
    return value;
}

uint32_t SubsystemInterface::get_mscom()
{
    return mscom;
}

uint32_t SubsystemInterface::get_smcom()
{
    return smcom;
}

uint32_t SubsystemInterface::get_msflag()
{
    return msflag;
}

uint32_t SubsystemInterface::get_smflag()
{
    return smflag;
}

uint32_t SubsystemInterface::get_control()
{
    return control;
}

void SubsystemInterface::set_mscom(uint32_t value)
{
    mscom = value;
}

void SubsystemInterface::set_smcom(uint32_t value)
{
    smcom = value;
}

void SubsystemInterface::set_msflag(uint32_t value)
{
    msflag |= value;
}

void SubsystemInterface::reset_msflag(uint32_t value)
{
    msflag &= ~value;
}

void SubsystemInterface::set_smflag(uint32_t value)
{
    smflag |= value;
}

void SubsystemInterface::reset_smflag(uint32_t value)
{
    smflag &= ~value;
}

void SubsystemInterface::set_control_EE(uint32_t value)
{
    if (!(value & 0x100))
        control &= ~0x100;
    else
        control |= 0x100;
}

void SubsystemInterface::set_control_IOP(uint32_t value)
{
    uint8_t bark = value & 0xF0;

    if (value & 0xA0)
    {
        control &= ~0xF000;
        control |= 0x2000;
    }

    if (control & bark)
        control &= ~bark;
    else
        control |= bark;
}

void SubsystemInterface::ee_log_sifrpc(uint32_t transfer_ptr, int len)
{
    if (len < 1)
        return;

    uint32_t call_ptr = transfer_ptr + (len * 0x10) - 0x10;
    uint32_t pkt = ee->read32(call_ptr);
    uint32_t rpc_func = ee->read32(pkt + 0x8);

    switch (rpc_func)
    {
        case 0x80000002:
            //INIT
            printf("[SIFRPC] sceSifInitRpc\n");
            for (auto it = rpc_servers.begin(); it != rpc_servers.end(); it++)
            {
                it->client_ptr = 0;
            }
            break;
        case 0x80000003:
            //RESET
            printf("[SIFRPC] sceSifResetIop\n");
            break;
        case 0x80000009:
            //BIND
        {
            printf("[SIFRPC] sceSifBindRpc\n");
            uint32_t module_id = ee->read32(pkt + 0x20);
            uint32_t client = ee->read32(pkt + 0x1C);
            if (!sifrpc_bind(module_id, client))
            {
                //Unable to find server, so install a custom one.
                sifrpc_register_server("CUSTOM", module_id,
                                       [] (SifRpcServer& server, uint32_t fno, uint32_t buffer, uint32_t size)
                                        { default_rpc(server, fno, buffer, size); });
                sifrpc_bind(module_id, client);
            }
        }
            break;
        case 0x8000000A:
            //CALL
            printf("[SIFRPC] sceSifCallRpc\n");
        {
            uint32_t client = ee->read32(pkt + 0x1C);
            uint32_t fno = ee->read32(pkt + 0x20);

            for (auto it = rpc_servers.begin(); it != rpc_servers.end(); it++)
            {
                if (it->client_ptr == client)
                {
                    it->rpc_func(*it, fno, ee->read32(transfer_ptr), 0);

                    //Check for FILEIO write - if fd is one, it's printing a message to STDOUT
                    /*if (it->module_id == 0x80000001 && fno == 3)
                    {
                        uint32_t arg_ptr = read32(dma_struct_ptr);
                        uint32_t fd = read32(arg_ptr);

                        if (fd == 1)
                        {
                            //HLE writing to STDOUT
                            e->ee_kputs(arg_ptr + 4);
                        }
                    }*/
                    return;
                }
            }
            printf("[SIFRPC] Unable to find server for client $%08X!\n", client);
        }
            break;
        default:
            printf("[SIFRPC] Unknown RPC function $%08X\n", rpc_func);
            break;
    }
}

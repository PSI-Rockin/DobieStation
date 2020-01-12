#include <iomanip>
#include <sstream>
#include "vu_disasm.hpp"

using namespace std;

namespace VU_Disasm
{

string get_field(uint8_t field)
{
    const static char vectors[] = {'w', 'z', 'y', 'x'};
    string out;
    for (int i = 3; i >= 0; i--)
    {
        if (field & (1 << i))
            out += vectors[i];
    }
    return out;
}

string get_fsf(int fsf)
{
    const static string vectors[] = {"x", "y", "z", "w"};
    return vectors[fsf];
}

string upper_simple(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 6) & 0x1F;
    uint32_t reg1 = (instr >> 11) & 0x1F;
    uint32_t reg2 = (instr >> 16) & 0x1F;
    uint32_t field = (instr >> 21) & 0xF;

    output << op << "." << get_field(field);
    output << " vf" << dest << ", vf" << reg1 << ", vf" << reg2;
    return output.str();
}

string upper_bc(const string op, uint32_t instr)
{
    stringstream output;
    string bc_field = get_fsf(instr & 0x3);
    uint32_t fd = (instr >> 6) & 0x1F;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;

    output << op << bc_field << "." << get_field(dest_field);
    output << " vf" << fd << ", vf" << fs << ", vf" << ft << bc_field;
    return output.str();
}

string upper_i(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t fd = (instr >> 6) & 0x1F;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;
    output << op << "i" << "." << get_field(dest_field);
    output << " vf" << fd << ", vf" << fs << ", I";
    return output.str();
}

string upper_q(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t fd = (instr >> 6) & 0x1F;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;
    output << op << "q" << "." << get_field(dest_field);
    output << " vf" << fd << ", vf" << fs << ", Q";
    return output.str();
}

string upper(uint32_t PC, uint32_t instr)
{
    uint8_t op = instr & 0x3F;
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            return upper_bc("add", instr);
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            return upper_bc("sub", instr);
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            return upper_bc("madd", instr);
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            return upper_bc("msub", instr);
        case 0x10:
        case 0x11:
        case 0x12:
        case 0x13:
            return upper_bc("max", instr);
        case 0x14:
        case 0x15:
        case 0x16:
        case 0x17:
            return upper_bc("mini", instr);
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            return upper_bc("mul", instr);
        case 0x1C:
            return upper_q("mul", instr);
        case 0x1D:
            return upper_i("max", instr);
        case 0x1E:
            return upper_i("mul", instr);
        case 0x1F:
            return upper_i("mini", instr);
        case 0x20:
            return upper_q("add", instr);
        case 0x21:
            return upper_q("madd", instr);
        case 0x22:
            return upper_i("add", instr);
        case 0x23:
            return upper_i("madd", instr);
        case 0x24:
            return upper_q("sub", instr);
        case 0x25:
            return upper_q("msub", instr);
        case 0x26:
            return upper_i("sub", instr);
        case 0x27:
            return upper_i("msub", instr);
        case 0x28:
            return upper_simple("add", instr);
        case 0x29:
            return upper_simple("madd", instr);
        case 0x2A:
            return upper_simple("mul", instr);
        case 0x2B:
            return upper_simple("max", instr);
        case 0x2C:
            return upper_simple("sub", instr);
        case 0x2D:
            return upper_simple("msub", instr);
        case 0x2E:
            return upper_simple("opmsub", instr);
        case 0x2F:
            return upper_simple("mini", instr);
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            return upper_special(PC, instr);
        default:
            return "[unknown upper]";
    }
}

string upper_acc(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    string field = "." + get_field(dest_field);
    output << op << field;
    output << " ACC, vf" << fs;
    output << ", vf" << ft;
    return output.str();
}

string upper_acc_bc(const string op, uint32_t instr)
{
    stringstream output;
    string bc_field = get_fsf(instr & 0x3);
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;

    output << op << bc_field << "." << get_field(dest_field);
    output << " ACC" << ", vf" << fs << ", vf" << ft << bc_field;
    return output.str();
}

string upper_acc_i(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;

    output << op << "i." << get_field(dest_field);
    output << " ACC" << ", vf" << fs << ", I";
    return output.str();
}

string upper_acc_q(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t dest_field = (instr >> 21) & 0xF;

    output << op << "q." << get_field(dest_field);
    output << " ACC" << ", vf" << fs << ", Q";
    return output.str();
}

string upper_conversion(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint32_t field = (instr >> 21) & 0xF;
    output << op << "." << get_field(field);
    output << " vf" << dest << ", vf" << source;
    return output.str();
}

string upper_special(uint32_t PC, uint32_t instr)
{
    uint16_t op = (instr & 0x3) | ((instr >> 4) & 0x7C);
    switch (op)
    {
        case 0x00:
        case 0x01:
        case 0x02:
        case 0x03:
            return upper_acc_bc("adda", instr);
        case 0x04:
        case 0x05:
        case 0x06:
        case 0x07:
            return upper_acc_bc("suba", instr);
        case 0x08:
        case 0x09:
        case 0x0A:
        case 0x0B:
            return upper_acc_bc("madda", instr);
        case 0x0C:
        case 0x0D:
        case 0x0E:
        case 0x0F:
            return upper_acc_bc("msuba", instr);
        case 0x10:
            return upper_conversion("itof0", instr);
        case 0x11:
            return upper_conversion("itof4", instr);
        case 0x12:
            return upper_conversion("itof12", instr);
        case 0x13:
            return upper_conversion("itof15", instr);
        case 0x14:
            return upper_conversion("ftoi0", instr);
        case 0x15:
            return upper_conversion("ftoi4", instr);
        case 0x16:
            return upper_conversion("ftoi12", instr);
        case 0x17:
            return upper_conversion("ftoi15", instr);
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
            return upper_acc_bc("mula", instr);
        case 0x1C:
            return upper_acc_q("mula", instr);
        case 0x1D:
            return upper_conversion("abs", instr);
        case 0x1E:
            return upper_acc_i("mula", instr);
        case 0x1F:
            return clip(instr);
        case 0x20:
            return upper_acc_q("adda", instr);
        case 0x21:
            return upper_acc_q("madda", instr);
        case 0x23:
            return upper_acc_i("madda", instr);
        case 0x25:
            return upper_acc_q("msuba", instr);
        case 0x26:
            return upper_acc_i("suba", instr);
        case 0x27:
            return upper_acc_i("msuba", instr);
        case 0x28:
            return upper_acc("adda", instr);
        case 0x29:
            return upper_acc("madda", instr);
        case 0x2A:
            return upper_acc("mula", instr);
        case 0x2C:
            return upper_acc("suba", instr);
        case 0x2D:
            return upper_acc("msuba", instr);
        case 0x2E:
            return opmula(instr);
        case 0x2F:
            return "nop";
        default:
            return "[unknown upper special]";
    }
}

string clip(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    output << "clipw.xyz vf" << fs << ", vf" << ft;
    return output.str();
}

string opmula(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    output << "opmula.xyz ACC, vf" << fs << ", vf" << ft;
    return output.str();
}

string loi(uint32_t lower)
{
    stringstream output;
    output << "loi 0x" << setfill('0') << setw(8) << hex << lower;
    return output.str();
}

bool is_branch(uint32_t instr)
{
    if (instr & (1 << 31))
        return false;
    uint8_t op = (instr >> 25) & 0x7F;
    switch (op)
    {
        case 0x20:
        case 0x21:
        case 0x28:
        case 0x29:
        case 0x2C:
        case 0x2D:
        case 0x2E:
        case 0x2F:
            return true;
        default:
            return false;
    }
}

string lower(uint32_t PC, uint32_t instr)
{
    if (instr == 0x8000033C)
        return "nop";
    if (instr & (1 << 31))
        return lower1(PC, instr);
    return lower2(PC, instr);
}

string lower1(uint32_t PC, uint32_t instr)
{
    uint8_t op = instr & 0x3F;
    switch (op)
    {
        case 0x30:
            return lower1_regi("iadd", instr);
        case 0x31:
            return lower1_regi("isub", instr);
        case 0x32:
            return lower1_immi("iaddi", instr);
        case 0x34:
            return lower1_regi("iand", instr);
        case 0x35:
            return lower1_regi("ior", instr);
        case 0x3C:
        case 0x3D:
        case 0x3E:
        case 0x3F:
            return lower1_special(PC, instr);
        default:
            return "[unknown lower1]";
    }
}

string lower1_regi(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 6) & 0x1F;
    uint32_t reg1 = (instr >> 11) & 0x1F;
    uint32_t reg2 = (instr >> 16) & 0x1F;
    output << op << " vi" << dest << ", vi" << reg1 << ", vi" << reg2;
    return output.str();
}

string lower1_immi(const string op, uint32_t instr)
{
    stringstream output;
    int32_t imm = (instr >> 6) & 0x1F;
    imm = ((int32_t)(int8_t)(imm << 3)) >> 3;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t dest = (instr >> 16) & 0x1F;
    output << op << " vi" << dest << ", vi" << source << ", 0x";
    output << setfill('0') << setw(4) << hex << imm;
    return output.str();
}

string lower1_special(uint32_t PC, uint32_t instr)
{
    uint8_t op = (instr & 0x3) | ((instr >> 4) & 0x7C);
    switch (op)
    {
        case 0x30:
            return move(instr);
        case 0x31:
            return mr32(instr);
        case 0x34:
            return lqi(instr);
        case 0x35:
            return sqi(instr);
        case 0x36:
            return lqd(instr);
        case 0x37:
            return sqd(instr);
        case 0x38:
            return div(instr);
        case 0x39:
            return vu_sqrt(instr);
        case 0x3A:
            return rsqrt(instr);
        case 0x3B:
            return "waitq";
        case 0x3C:
            return mtir(instr);
        case 0x3D:
            return mfir(instr);
        case 0x3E:
            return ilwr(instr);
        case 0x3F:
            return iswr(instr);
        case 0x40:
            return rnext(instr);
        case 0x41:
            return rget(instr);
        case 0x42:
            return rinit(instr);
        case 0x43:
            return rxor(instr);
        case 0x64:
            return mfp(instr);
        case 0x68:
            return xtop(instr);
        case 0x69:
            return xitop(instr);
        case 0x6C:
            return xgkick(instr);
        case 0x70:
            return esadd(instr);
        case 0x71:
            return ersadd(instr);
        case 0x72:
            return eleng(instr);
        case 0x73:
            return erleng(instr);
        case 0x74: 
            return eatanxy(instr);
        case 0x75:
            return eatanxz(instr);
        case 0x76:
            return esum(instr);
        case 0x78:
            return esqrt(instr);
        case 0x79:
            return ersqrt(instr);
        case 0x7A:
            return ercpr(instr);
        case 0x7B:
            return "waitp";
        case 0x7D:
            return eatan(instr);
        case 0x7E:
            return eexp(instr);
        default:
            return "[unknown lower1 special]";
    }
}

string move(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "move." << get_field(field);
    output << " vf" << dest << ", vf" << source;
    return output.str();
}

string mr32(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "mr32." << get_field(field);
    output << " vf" << dest << ", vf" << source;
    return output.str();
}

string lqi(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "lqi." << get_field(field);
    output << " vf" << ft << ", (vi" << is << "++)";
    return output.str();
}

string sqi(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "sqi." << get_field(field);
    output << " vf" << fs << ", (vi" << it << "++)";
    return output.str();
}

string lqd(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "lqd." << get_field(field);
    output << " vf" << ft << ", (--vi" << is << ")";
    return output.str();
}

string sqd(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "sqd." << get_field(field);
    output << " vf" << fs << ", (--vi" << it << ")";
    return output.str();
}

string div(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    string fsf = get_fsf((instr >> 21) & 0x3);
    string ftf = get_fsf((instr >> 23) & 0x3);
    output << "div Q, vf" << fs << fsf << ", vf" << ft << ftf;
    return output.str();
}

string vu_sqrt(uint32_t instr)
{
    stringstream output;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t ftf = (instr >> 23) & 0x3;
    output << "sqrt Q, vf" << ft << "." << get_fsf(ftf);
    return output.str();
}

string rsqrt(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t ftf = (instr >> 23) & 0x3;
    output << "rsqrt Q, vf" << fs << "." << get_fsf(fsf) << ", vf" << ft << "." << get_fsf(ftf);
    return output.str();
}

string mtir(uint32_t instr)
{
    stringstream output;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t fsf = (instr >> 21) & 0x3;
    output << "mtir vi" << it << ", vf" << fs << "." << get_fsf(fsf);
    return output.str();
}

string mfir(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t dest_field = (instr >> 21) & 0xF;
    output << "mfir." << get_field(dest_field) << " vf" << ft << ", vi" << is;
    return output.str();
}

string ilwr(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "ilwr." << get_field(field);
    output << " vi" << it << ", (vi" << is << ")";
    return output.str();
}

string iswr(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "iswr." << get_field(field);
    output << " vi" << it << ", (vi" << is << ")";
    return output.str();
}

string rnext(uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "rnext." << get_field(field);
    output << " vf" << dest << ", R";
    return output.str();
}

string rget(uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "rget." << get_field(field);
    output << " vf" << dest << ", R";
    return output.str();
}

string rinit(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t fsf = (instr >> 21) & 0x3;
    output << "rinit R,";
    output << " vf" << source << "."<< get_fsf(fsf);
    return output.str();
}

string rxor(uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "rxor." << get_field(field);
    output << " R, vf" << dest;
    return output.str();
}

string mfp(uint32_t instr)
{
    stringstream output;
    uint32_t dest = (instr >> 16) & 0x1F;
    uint32_t field = (instr >> 21) & 0xF;
    output << "mfp." << get_field(field);
    output << " vf" << dest << ", P";
    return output.str();
}

string xtop(uint32_t instr)
{
    stringstream output;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "xtop vi" << it;
    return output.str();
}

string xitop(uint32_t instr)
{
    stringstream output;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "xitop vi" << it;
    return output.str();
}

string xgkick(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    output << "xgkick vi" << is;
    return output.str();
}

string esadd(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "esadd P, vf" << source;
    return output.str();
}

string ersadd(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "ersadd P, vf" << source;
    return output.str();
}

string eleng(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "eleng P, vf" << source;
    return output.str();
}

string esum(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "esum P, vf" << source;
    return output.str();
}

string ercpr(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "ercpr P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string erleng(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "erleng P, vf" << source;
    return output.str();
}

string esqrt(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "esqrt P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string ersqrt(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "ersqrt P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string esin(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "esin P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string eatan(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "eatan P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string eatanxy(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "eatanxy P, vf" << source;
    return output.str();
}

string eatanxz(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "eatanxz P, vf" << source;
    return output.str();
}

string eexp(uint32_t instr)
{
    stringstream output;
    uint32_t source = (instr >> 11) & 0x1F;
    output << "eexp P, vf" << source << "." << get_fsf((instr >> 21) & 0x3);
    return output.str();
}

string lower2(uint32_t PC, uint32_t instr)
{
    uint8_t op = (instr >> 25) & 0x7F;
    switch (op)
    {
        case 0x00:
            return lq(instr);
        case 0x01:
            return sq(instr);
        case 0x04:
            return loadstore_imm("ilw", instr);
        case 0x05:
            return loadstore_imm("isw", instr);
        case 0x08:
            return arithu("iaddiu", instr);
        case 0x09:
            return arithu("isubiu", instr);
        case 0x10:
            return fceq(instr);
        case 0x11:
            return fcset(instr);
        case 0x12:
            return fcand(instr);
        case 0x13:
            return fcor(instr);
        case 0x14:
            return fseq(instr);
        case 0x15:
            return fsset(instr);
        case 0x16:
            return fsand(instr);
        case 0x17:
            return fsor(instr);
        case 0x18:
            return fmeq(instr);
        case 0x1A:
            return fmand(instr);
        case 0x1B:
            return fmor(instr);
        case 0x1C:
            return fcget(instr);
        case 0x20:
            return b(PC, instr);
        case 0x21:
            return bal(PC, instr);
        case 0x24:
            return jr(instr);
        case 0x25:
            return jalr(instr);
        case 0x28:
            return branch("ibeq", PC, instr);
        case 0x29:
            return branch("ibne", PC, instr);
        case 0x2C:
            return branch_zero("ibltz", PC, instr);
        case 0x2D:
            return branch_zero("ibgtz", PC, instr);
        case 0x2E:
            return branch_zero("iblez", PC, instr);
        case 0x2F:
            return branch_zero("ibgez", PC, instr);
        default:
            return "[unknown lower2]";
    }
}

string loadstore_imm(const string op, uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << op << "." << get_field(field);
    output << " vi" << it << ", 0x";
    output << setfill('0') << setw(4) << hex << imm;
    output << "(vi" << is << ")";
    return output.str();
}

string arithu(const string op, uint32_t instr)
{
    stringstream output;
    uint32_t imm = instr & 0x7FF;
    imm |= ((instr >> 21) & 0xF) << 11;
    uint32_t source = (instr >> 11) & 0x1F;
    uint32_t dest = (instr >> 16) & 0x1F;
    output << op << " vi" << dest << ", vi" << source << ", 0x";
    output << setfill('0') << setw(4) << hex << imm;
    return output.str();
}

string branch(const string op, uint32_t PC, uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint32_t addr = PC + imm + 8;
    output << op << " vi" << it << ", vi" << is << ", $";
    output << setfill('0') << setw(4) << hex << addr;
    return output.str();
}

string branch_zero(const string op, uint32_t PC, uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    uint32_t reg = (instr >> 11) & 0x1F;

    uint32_t addr = PC + imm + 8;
    output << op << " vi" << reg << ", $" << setfill('0') << setw(4) << hex << addr;
    return output.str();
}

string lq(uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t ft = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "lq." << get_field(field) << " vf" << ft << ", 0x";
    output << setfill('0') << setw(4) << hex << imm;
    output << "(vi" << dec << is << ")";
    return output.str();
}

string sq(uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    uint32_t fs = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    uint8_t field = (instr >> 21) & 0xF;
    output << "sq." << get_field(field) << " vf" << fs << ", 0x";
    output << setfill('0') << setw(4) << hex << imm;
    output << "(vi" << dec << it << ")";
    return output.str();
}

string fceq(uint32_t instr)
{
    stringstream output;
    uint32_t imm = instr & 0xFFFFFF;
    output << "fceq vi1, 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fcset(uint32_t instr)
{
    stringstream output;
    uint32_t imm = instr & 0xFFFFFF;
    output << "fcset 0x" << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fcand(uint32_t instr)
{
    stringstream output;
    uint32_t imm = instr & 0xFFFFFF;
    output << "fcand vi1, 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fcor(uint32_t instr)
{
    stringstream output;
    uint32_t imm = instr & 0xFFFFFF;
    output << "fcor vi1, 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fseq(uint32_t instr)
{
    stringstream output;
    uint32_t imm = ((instr >> 10) & 0x800) | (instr & 0x7FF);
    uint32_t dest = (instr >> 16) & 0x1F;
    output << "fseq vi" << dest << ", 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fsset(uint32_t instr)
{
    stringstream output;
    uint32_t imm = ((instr >> 10) & 0x800) | (instr & 0x7FF);
    output << "fsset 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fsand(uint32_t instr)
{
    stringstream output;
    uint32_t imm = ((instr >> 10) & 0x800) | (instr & 0x7FF);
    uint32_t dest = (instr >> 16) & 0x1F;
    output << "fsand vi" << dest << ", 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fsor(uint32_t instr)
{
    stringstream output;
    uint32_t imm = ((instr >> 10) & 0x800) | (instr & 0x7FF);
    uint32_t dest = (instr >> 16) & 0x1F;
    output << "fsor vi" << dest << ", 0x";
    output << setfill('0') << setw(8) << hex << imm;
    return output.str();
}

string fmeq(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "fmeq vi" << it << ", vi" << is;
    return output.str();
}

string fmand(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "fmand vi" << it << ", vi" << is;
    return output.str();
}

string fmor(uint32_t instr)
{
    stringstream output;
    uint32_t is = (instr >> 11) & 0x1F;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "fmor vi" << it << ", vi" << is;
    return output.str();
}

string fcget(uint32_t instr)
{
    stringstream output;
    uint32_t it = (instr >> 16) & 0x1F;
    output << "fcget vi" << it;
    return output.str();
}

string b(uint32_t PC, uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;

    uint32_t addr = PC + imm + 8;
    output << "b $" << setfill('0') << setw(4) << hex << addr;
    return output.str();
}

string bal(uint32_t PC, uint32_t instr)
{
    stringstream output;
    int32_t imm = instr & 0x7FF;
    imm = ((int16_t)(imm << 5)) >> 5;
    imm *= 8;
    uint32_t link_reg = (instr >> 16) & 0x1F;

    uint32_t addr = PC + imm + 8;
    output << "bal vi" << link_reg << ", $" << setfill('0') << setw(4) << hex << addr;
    return output.str();
}

string jr(uint32_t instr)
{
    stringstream output;
    uint32_t addr_reg = (instr >> 11) & 0x1F;
    output << "jr vi" << addr_reg;
    return output.str();
}

string jalr(uint32_t instr)
{
    stringstream output;
    uint32_t addr_reg = (instr >> 11) & 0x1F;
    uint32_t link_reg = (instr >> 16) & 0x1F;
    output << "jalr vi" << link_reg << ", vi" << addr_reg;
    return output.str();
}

}; //VU_Disasm

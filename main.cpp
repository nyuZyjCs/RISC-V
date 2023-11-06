#include<iostream>
#include<string>
#include<vector>
#include<bitset>
#include<unordered_map>
#include<functional>
#include<fstream>
#include<filesystem>

#ifdef _WIN32
const char PATH_SEP = '\\';
#else
const char PATH_SEP = '/';
#endif

using namespace std;
namespace fs = filesystem;

#define MemSize 1000 // memory size, in reality, the memory size should be 2^32, but for this lab, for the space resaon, we keep it as this large number, but the memory is still 32-bit addressable.
#define RegSize 32 // register size


// 定义一个函数指针类型，用于指向ALU操作的函数
typedef function<bitset<32>(bitset<32>, bitset<32>)> AluFunc;

// 定义ALU操作的映射表
unordered_map<std::string, AluFunc> aluOperation = {
    {"000", [](std::bitset<32> a, std::bitset<32> b) { return a.to_ulong() + b.to_ulong(); }},
    {"001", [](std::bitset<32> a, std::bitset<32> b) { return a.to_ulong() - b.to_ulong(); }},
    {"100", [](std::bitset<32> a, std::bitset<32> b) { return a.to_ulong() ^ b.to_ulong(); }},
    {"110", [](std::bitset<32> a, std::bitset<32> b) { return a.to_ulong() | b.to_ulong(); }},
    {"111", [](std::bitset<32> a, std::bitset<32> b) { return a.to_ulong() & b.to_ulong(); }}
};

struct IFStruct {
    bitset<32>  PC;
    bool        nop;
};

struct IDStruct {
    bitset<32>  Instr;
    bool        nop;
};

struct EXStruct {
    bitset<32>  Read_data1;
    bitset<32>  Read_data2;
    bitset<12>  Imm;
    bitset<5>   rs1;
    bitset<5>   rs2;
    bitset<5>   rd;
    bool        is_I_type;
    bool        rd_mem;
    bool        wrt_mem;
    bitset<3>   alu_op;     //000: add   001: sub   100: xor   110: or   111: and
    bool        wrt_enable;
    bool        nop;
};

struct MEMStruct {
    bitset<32>  ALUresult;
    bitset<32>  Store_data;
    bitset<5>   rs1;
    bitset<5>   rs2;
    bitset<5>   rd;
    bool        rd_mem;
    bool        wrt_mem;
    bool        wrt_enable;
    bool        nop;
};

struct WBStruct {
    bitset<32>  Wrt_data;
    bitset<5>   rs1;
    bitset<5>   rs2;
    bitset<5>   rd;
    bool        wrt_enable;
    bool        nop;
};

struct stateStruct {
    IFStruct    IF;
    IDStruct    ID;
    EXStruct    EX;
    MEMStruct   MEM;
    WBStruct    WB;
};

template <size_t bitSize>
bitset<32> sign_extend(bitset<bitSize> Imm) {
    bitset<32> result;
    if (Imm[bitSize - 1]) {  // If the MSB is 1
        bitset<32> mask = (~bitset<32>(0)) << bitSize;
        result = mask | bitset<32>(Imm.to_ulong());
    } else {       // If the MSB is 0
        result = bitset<32>(Imm.to_ulong());
    }
    return result;
}

//string convertIoToOp(const string& inputPath) {
//    size_t lastSlashPos = inputPath.rfind('/');
//    if (lastSlashPos == std::string::npos || lastSlashPos == inputPath.length() - 1) {
//        return "";   //if wrong space, return ""
//    }
//
//    string testcase = inputPath.substr(lastSlashPos + 1);
//
//    string outputPath = "../output_yz10617";
//}

class InsMem {
public:
    string id, ioDir;
    InsMem(string name, string ioDir) : id(name) {
        IMem.resize(MemSize);
        string line;
        fs::path filePath = fs::path(ioDir) / "imem.txt";

        ifstream imem(filePath);
        int i=0;
        if (imem.is_open()) {
            while (getline(imem,line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                IMem[i] = bitset<8>(line);
                i++;
            }
        }
        else cout << "Unable to open IMEM input file.";
        imem.close();
    }

    bitset<32> readInstr(bitset<32> ReadAddress) {
        // read instruction memory
        // return bitset<32> val
        size_t address = ReadAddress.to_ulong();
        if (address + 3 >= MemSize) {
            cout << "Address out of bounds!" << endl;
            return bitset<32>(0);
        }

        string instrution;
        for (int i = 0; i < 4; ++i) {
            instrution += IMem[address + i].to_string();
        }
        return bitset<32>(instrution);
    }

private:
    vector<bitset<8>> IMem;
};

class DataMem {
public:
    string id;
    fs::path opFilePath, ioDir;
    DataMem(string name, fs::path ioDir) : id{name}, ioDir{ioDir} {
        DMem.resize(MemSize);
        fs::path dmemPath = fs::path(ioDir) / "dmem.txt";
        fs::path outputDir = ioDir.parent_path().parent_path() / "output" / ioDir.filename();
        fs::create_directories(outputDir);
        opFilePath = outputDir / (name + "_DMEMResult.txt");

        ifstream dmem(dmemPath);
        string line;
        int i = 0;
        if (dmem.is_open()) {
            while (getline(dmem,line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                DMem[i] = bitset<8>(line);
                i++;
            }
        }
        else cout << "Unable to open DMEM input file.";
        dmem.close();
    }

    bitset<32> readDataMem(bitset<32> Address) {
        // read data memory
        // return bitset<32> val
        size_t address = Address.to_ulong();
        if (address + 3 >= MemSize) {
            cout << "MemAddress out of bounds!" << endl;
            return bitset<32>(0);
        }

        string data;
        for (int i = 0; i < 4; ++i) {
            data += DMem[address + i].to_string();
        }
        return bitset<32>(data);
    }

    void writeDataMem(bitset<32> Address, bitset<32> WriteData) {
        // write into memory
        size_t address = Address.to_ulong();

        if (address + 3 >= MemSize) {
            cout << "Address out of bounds!" << endl;
            return;
        }

        for (int i = 0; i < 4; ++i) {
            DMem[address + i] = bitset<8>(WriteData.to_string().substr(i * 8, 8));
        }
    }

    void outputDataMem() {
        ofstream dmemout;
        dmemout.open(opFilePath, std::ios_base::trunc);
        if (dmemout.is_open()) {
            for (int j = 0; j< 1000; j++) {
                dmemout << DMem[j] << endl;
            }
        }
        else cout << "Unable to open " << id << " DMEM result file." << endl;
        dmemout.close();
    }

private:
    vector<bitset<8>> DMem;
};

class RegisterFile {
public:
    fs::path opFilePath;
    RegisterFile(fs::path ioDir) {
        fs::path outputDir = ioDir.parent_path().parent_path() / "output" / ioDir.filename();
        fs::create_directories(outputDir);
        opFilePath = outputDir /  "RFResult.txt";
        Registers.resize(RegSize);
        Registers[0] = bitset<32> (0);
    }

    bitset<32> readRF(bitset<5> Reg_addr) {
        // Fill in
        size_t address = Reg_addr.to_ulong();
        if (address >= RegSize) {
            cout << "Invalid register address!" << endl;
            return bitset<32> (0);
        }
        return Registers[address];
    }

    void writeRF(bitset<5> Reg_addr, bitset<32> Wrt_reg_data) {
        // Fill in
        size_t address = Reg_addr.to_ulong();
        if (address == 0) {
            cout << "Register 0 always remains 0 and cannot be changed." << endl;
            return;
        } else if (address >= RegSize) {
            cout << "Invalid register address!" << endl;
        }
        Registers[address] = Wrt_reg_data;
    }

    void outputRF(int cycle) {
        ofstream rfout;
        rfout.open(opFilePath, (cycle == 0) ? std::ios_base::trunc : std::ios_base::app);

        if (rfout.is_open()) {
            rfout << "State of RF after executing cycle:\t" << cycle << endl;
            for (int j = 0; j < RegSize; j++) {
                rfout << Registers[j] << endl;
            }
        }
        else cout << "Unable to open RF output file." << endl;
        rfout.close();
    }

private:
    vector<bitset<32>> Registers;
};

class Core {
public:
    RegisterFile myRF;
    uint32_t cycle = 0;
    uint32_t instruction_num = 0;
    bool halted = false;
    fs::path ioDir;
    struct stateStruct state, nextState;
    InsMem& ext_imem;
    DataMem& ext_dmem;

    Core(fs::path ioDir, InsMem &imem, DataMem &dmem): myRF(ioDir), ioDir{ioDir}, ext_imem {imem}, ext_dmem {dmem} {}

    virtual void step() {}

    virtual void printState() {}
};

class SingleStageCore : public Core {
public:
    SingleStageCore(fs::path ioDir, InsMem &imem, DataMem &dmem): Core(ioDir, imem, dmem) {
        fs::path outputDir = ioDir.parent_path().parent_path() / "output" / ioDir.filename();
        fs::create_directories(outputDir);
        opFilePath = outputDir;
    }

    void step() {
        /* Your implementation*/
        // Instruction Fetch
        if (!state.IF.nop) {
            state.ID.Instr = ext_imem.readInstr(state.IF.PC);
            if (state.ID.Instr != 0xffffffff) {
                nextState.IF.PC = bitset<32>(state.IF.PC.to_ulong() + 4);
                nextState.IF.nop = false;
            } else {
                state.IF.nop = true;
            }
        }

        // Instruction Decode
        string instruction = state.ID.Instr.to_string();
        instruction_num++;

        string opcode = instruction.substr(25, 7);
        state.EX.rd = bitset<5>(instruction.substr(20, 5));  //rd
        string func3 = instruction.substr(17, 3);
        state.EX.rs1 = bitset<5>(instruction.substr(12, 5)); //rs1
        state.EX.rs2 = bitset<5>(instruction.substr(7, 5));  //rs2
        string func7 = instruction.substr(0, 7);


        state.EX.is_I_type = false;
        state.EX.rd_mem = false;
        state.EX.wrt_mem = false;
        state.EX.wrt_enable = false;
        if (opcode == "0110011") { //R-type
            state.EX.Read_data1 = myRF.readRF(state.EX.rs1);
            state.EX.Read_data2 = myRF.readRF(state.EX.rs2);
            state.EX.wrt_enable = true;
            if (func3 == "000" && func7 == "0000000") { //add
                state.EX.alu_op = bitset<3>("000");
            } else if (func3 == "000" && func7 == "0100000") { //sub
                state.EX.alu_op = bitset<3>("001");
            } else if (func3 == "100" && func7 == "0000000") { //XOR
                state.EX.alu_op = bitset<3>("100");
            } else if (func3 == "110" && func7 == "0000000") { //OR
                state.EX.alu_op = bitset<3>("110");
            } else if (func3 == "111" && func7 == "0000000") { //AND
                state.EX.alu_op = bitset<3>("111");
            }
        }
        else if (opcode == "0010011" || opcode == "0000011") { //I-type
            state.EX.is_I_type = true;
            state.EX.Imm = bitset<12>(func7 + state.EX.rs2.to_string());
            state.EX.Read_data1 = myRF.readRF(state.EX.rs1);
            state.EX.wrt_enable = true;
            if (func3 == "000") { //addi
                state.EX.alu_op = bitset<3>("000");
                if (opcode == "0000011") { //lw
                    state.EX.rd_mem = true;
                }
            } else if (func3 == "100") { //XORI
                state.EX.alu_op = bitset<3>("100");
            } else if (func3 == "110") { //ORI
                state.EX.alu_op = bitset<3>("110");
            } else if (func3 == "111") { //ANDI
                state.EX.alu_op = bitset<3>("111");
            }
        }
        else if (opcode == "0100011") { //S-type
            state.EX.Read_data1 = myRF.readRF(state.EX.rs1);
            state.EX.Read_data2 = myRF.readRF(state.EX.rs2);
            state.EX.Imm = bitset<12>(func7 + state.EX.rd.to_string());
            state.EX.alu_op = bitset<3>("000");
            state.EX.is_I_type = true;
            state.EX.wrt_mem = true;
        }
        else if (opcode == "1101111") { //J-type
            string firstPart = func7 + state.EX.rs2.to_string();
            string secondPart = state.EX.rs1.to_string() + func3;
            bitset<20> jImm = bitset<20>(firstPart.substr(0, 1) + secondPart +
                                         firstPart.substr(1, 10) + firstPart.substr(11, 1));
            nextState.IF.PC = state.IF.PC.to_ulong() + sign_extend(jImm).to_ulong();
            state.EX.wrt_enable = true;
        }
        else if (opcode == "1100011") { //B-type
            state.EX.Read_data1 = myRF.readRF(state.EX.rs1);
            state.EX.Read_data2 = myRF.readRF(state.EX.rs2);
            string temp = state.EX.rd.to_string();
            bitset<13> bImm = bitset<13>(func7.substr(0, 1) + temp.substr(temp.size() - 1, 1) +
                                      func7.substr(1) + temp.substr(0, 4) + "0");
            if (func3 == "000") { //beq
                if (state.EX.Read_data1 == state.EX.Read_data2) {
                    nextState.IF.PC = state.IF.PC.to_ulong() + sign_extend(bImm).to_ulong();
                }
            }
            else if (func3 == "001") { //bne
                if (state.EX.Read_data1 != state.EX.Read_data2) {
                    nextState.IF.PC = state.IF.PC.to_ulong() + sign_extend(bImm).to_ulong();
                }
            }
            state.EX.nop = true;
        }

        //Ex
        state.MEM.rs1 = state.EX.rs1;
        state.MEM.rs2 = state.EX.rs2;
        state.MEM.rd = state.EX.rd;
        state.MEM.rd_mem = state.EX.rd_mem;
        state.MEM.wrt_mem = state.EX.wrt_mem;
        state.MEM.wrt_enable = state.EX.wrt_enable;
        state.MEM.nop = state.EX.nop;
        if (!state.EX.nop) {
            bitset<32> aluResult;
            string alu_op = state.EX.alu_op.to_string();
            if (state.EX.is_I_type) {
                bitset<32> extendedImm = sign_extend(state.EX.Imm);
                if (aluOperation.find(alu_op) != aluOperation.end()) {
                    aluResult = aluOperation[alu_op](state.EX.Read_data1, extendedImm);
                }
                state.MEM.Store_data = state.EX.Read_data2;
            }
            else {
                if (aluOperation.find(alu_op) != aluOperation.end()) {
                    aluResult = aluOperation[alu_op](state.EX.Read_data1, state.EX.Read_data2);
                }
            }
            state.MEM.ALUresult = aluResult;
        }
        if (opcode == "1101111") state.MEM.ALUresult = state.IF.PC.to_ulong() + 4;

        //Mem
        state.WB.rs1 = state.MEM.rs1;
        state.WB.rs2 = state.MEM.rs2;
        state.WB.rd = state.MEM.rd;
        state.WB.wrt_enable = state.MEM.wrt_enable;
        state.WB.nop = state.MEM.nop;
        if (!state.MEM.nop) {
            if (state.MEM.rd_mem) {
                state.WB.Wrt_data = ext_dmem.readDataMem(state.MEM.ALUresult);
            }
            else if (state.MEM.wrt_mem) {
                ext_dmem.writeDataMem(state.MEM.ALUresult, state.MEM.Store_data);
            }
            else if (state.WB.wrt_enable){
                state.WB.Wrt_data = state.MEM.ALUresult;
            }
        }

        //WB
        if (!state.WB.nop && state.WB.wrt_enable) {
            myRF.writeRF(state.WB.rd, state.WB.Wrt_data);
        }

        if (state.IF.nop){
            halted = true;
            performance_analysis(instruction_num, cycle + 1);
        }



        myRF.outputRF(cycle); // dump RF
        printState(state, cycle); //print states after executing cycle 0, cycle 1, cycle 2 ...


        state = nextState; // The end of the cycle and updates the current state with the values calculated in this cycle
        cycle++;
    }

    void printState(stateStruct state, int cycle) {
        ofstream printstate;
        cout << opFilePath << endl;
        printstate.open(opFilePath / "StateResult_SS.txt", (cycle == 0) ? ios_base::trunc : ios_base::app);

        if (printstate.is_open()) {
            printstate<<"----------------------------------------------------------------------"<<endl;
            printstate<<"State after executing cycle:\t"<<cycle<<endl;

            printstate<<"IF.PC:\t"<<state.IF.PC.to_ulong()<<endl;
            printstate<<"IF.nop:\t"<<state.IF.nop<<endl;
        }
        else cout<<"Unable to open SS StateResult output file." << endl;
        printstate.close();
    }

    void performance_analysis(int instruction_num, int cycle) {
        ofstream printstate;
        printstate.open(opFilePath / "PerformanceMetrics.txt", std::ios_base::trunc);
        if (printstate.is_open()) {
            printstate << "Performance of Single Stage:\t" << endl;

            printstate << "Cycles:\t" << cycle << endl;
            printstate << "Instructions:\t"<< instruction_num <<endl;
            printstate << "CPI:\t" << cycle / instruction_num << endl;
            printstate << "IPC:\t" << instruction_num / cycle << endl;
        }
        else cout<<"Unable to open PerformanceMetrics output file." << endl;
        printstate.close();
    }
private:
    fs::path opFilePath;
};

class FiveStageCore : public Core{
public:

    FiveStageCore(string ioDir, InsMem &imem, DataMem &dmem): Core(ioDir + "\\FS_", imem, dmem), opFilePath(ioDir + "\\StateResult_FS.txt") {}

    void step() {
        /* Your implementation */
        /* --------------------- WB stage --------------------- */



        /* --------------------- MEM stage -------------------- */



        /* --------------------- EX stage --------------------- */



        /* --------------------- ID stage --------------------- */



        /* --------------------- IF stage --------------------- */


        halted = true;
        if (state.IF.nop && state.ID.nop && state.EX.nop && state.MEM.nop && state.WB.nop)
            halted = true;

        myRF.outputRF(cycle); // dump RF
        printState(nextState, cycle); //print states after executing cycle 0, cycle 1, cycle 2 ...

        state = nextState; //The end of the cycle and updates the current state with the values calculated in this cycle
        cycle++;
    }

    void printState(stateStruct state, int cycle) {
        ofstream printstate;
        if (cycle == 0)
            printstate.open(opFilePath, std::ios_base::trunc);
        else
            printstate.open(opFilePath, std::ios_base::app);
        if (printstate.is_open()) {
            printstate<<"State after executing cycle:\t"<<cycle<<endl;

            printstate<<"IF.PC:\t"<<state.IF.PC.to_ulong()<<endl;
            printstate<<"IF.nop:\t"<<state.IF.nop<<endl;

            printstate<<"ID.Instr:\t"<<state.ID.Instr<<endl;
            printstate<<"ID.nop:\t"<<state.ID.nop<<endl;

            printstate<<"EX.Read_data1:\t"<<state.EX.Read_data1<<endl;
            printstate<<"EX.Read_data2:\t"<<state.EX.Read_data2<<endl;
            printstate<<"EX.Imm:\t"<<state.EX.Imm<<endl;
            printstate<<"EX.Rs:\t"<<state.EX.rs1<<endl;
            printstate<<"EX.Rt:\t"<<state.EX.rs2<<endl;
            printstate<<"EX.Wrt_reg_addr:\t"<<state.EX.rd<<endl;
            printstate<<"EX.is_I_type:\t"<<state.EX.is_I_type<<endl;
            printstate<<"EX.rd_mem:\t"<<state.EX.rd_mem<<endl;
            printstate<<"EX.wrt_mem:\t"<<state.EX.wrt_mem<<endl;
            printstate<<"EX.alu_op:\t"<<state.EX.alu_op<<endl;
            printstate<<"EX.wrt_enable:\t"<<state.EX.wrt_enable<<endl;
            printstate<<"EX.nop:\t"<<state.EX.nop<<endl;

            printstate<<"MEM.ALUresult:\t"<<state.MEM.ALUresult<<endl;
            printstate<<"MEM.Store_data:\t"<<state.MEM.Store_data<<endl;
            printstate<<"MEM.Rs:\t"<<state.MEM.rs1<<endl;
            printstate<<"MEM.Rt:\t"<<state.MEM.rs2<<endl;
            printstate<<"MEM.Wrt_reg_addr:\t"<<state.MEM.rd<<endl;
            printstate<<"MEM.rd_mem:\t"<<state.MEM.rd_mem<<endl;
            printstate<<"MEM.wrt_mem:\t"<<state.MEM.wrt_mem<<endl;
            printstate<<"MEM.wrt_enable:\t"<<state.MEM.wrt_enable<<endl;
            printstate<<"MEM.nop:\t"<<state.MEM.nop<<endl;

            printstate<<"WB.Wrt_data:\t"<<state.WB.Wrt_data<<endl;
            printstate<<"WB.Rs:\t"<<state.WB.rs1<<endl;
            printstate<<"WB.Rt:\t"<<state.WB.rs2<<endl;
            printstate<<"WB.Wrt_reg_addr:\t"<<state.WB.rd<<endl;
            printstate<<"WB.wrt_enable:\t"<<state.WB.wrt_enable<<endl;
            printstate<<"WB.nop:\t"<<state.WB.nop<<endl;
        }
        else cout<<"Unable to open FS StateResult output file." << endl;
        printstate.close();
    }
private:
    string opFilePath;
};

int main(int argc, char* argv[]) {

    fs::path ioDir;
    if (argc == 1) {
        cout << "(Example)Enter path containing the memory files: ../input/testcase3" << endl;
        cout << "Enter path containing the memory files: ";
        string inputPath;
        cin >> inputPath;
        ioDir = inputPath;
//        ioDir = "../input/testcase0";
    }
    else if (argc > 2) {
        cout << "Invalid number of arguments. Machine stopped." << endl;
        return -1;
    }
    else {
        ioDir = argv[1];
        cout << "IO Directory: " << ioDir << endl;
    }

    InsMem imem = InsMem("Imem", ioDir);
    DataMem dmem_ss = DataMem("SS", ioDir);
//    DataMem dmem_fs = DataMem("FS", ioDir);

    SingleStageCore SSCore(ioDir, imem, dmem_ss);
//    FiveStageCore FSCore(ioDir, imem, dmem_fs);

    while (1) {
        if (!SSCore.halted)
            SSCore.step();

//        if (!FSCore.halted)
//            FSCore.step();

//        if (SSCore.halted && FSCore.halted)
//            break;

        if (SSCore.halted) break;
    }

    // dump SS and FS data mem.
    dmem_ss.outputDataMem();
//    dmem_fs.outputDataMem();

    return 0;
}
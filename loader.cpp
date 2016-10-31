#include <iostream>
#include <elf.h>
#include <fstream>
#include <vector>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
using namespace std;

const char * RegName [32]={"zero","ra","sp","gp","tp","t0","t1","t2","s0","s1","a0","a1","a2","a3","a4","a5","a6","a7","s2","s3","s4","s5","s6","s7","s8","s9","s10","s11","t3","t4","t5","t6"};

struct MemoryBlock 
{
    char * Content;
    uint64_t StartAddr;
    uint64_t EndAddr;
    MemoryBlock(char * Content_, uint64_t StartAddr_, uint64_t EndAddr_)
    {
        Content=Content_;
        StartAddr=StartAddr_;
        EndAddr=EndAddr_;
    }
};

void Error(const char * msg)
{
    printf("%s", msg);
    exit(1);
}

class VirtualMemory
{
    vector<MemoryBlock*>blocks;
    char * getPaddr(uint64_t Vaddr)
    {
        int l=blocks.size();
        for(int i=0; i<l; i++)
        {
            if(Vaddr>=blocks[i]->StartAddr && Vaddr<blocks[i]->EndAddr)
            {
                return blocks[i]->Content + (Vaddr-blocks[i]->StartAddr);
            }
        }
        Error("Memory error");
    }
    public:
    
    void load(char * Content,uint64_t MemSize,uint64_t StartAddr)
    {
        blocks.push_back(new MemoryBlock(Content,StartAddr,StartAddr+MemSize));
    }
    
    uint8_t ReadByte(uint64_t Vaddr)
    {
        return *(uint8_t *)getPaddr(Vaddr);
    }
    
    void WriteByte(uint64_t Vaddr, uint8_t Byte)
    {   
        *(uint8_t *)getPaddr(Vaddr)=Byte;
    }
    
    uint16_t ReadHalfword(uint64_t Vaddr)
    {
        return *(uint16_t *)getPaddr(Vaddr);
    }
    
    void WriteHalfword(uint64_t Vaddr,uint16_t Halfword)
    {  
       *(uint16_t *)getPaddr(Vaddr)=Halfword;
    }
    
    uint32_t ReadWord(uint64_t Vaddr)
    {
        return *(uint32_t *)getPaddr(Vaddr);
    }
    
    void WriteWord(uint64_t Vaddr,uint32_t Word)
    {
        *(uint32_t *)getPaddr(Vaddr)=Word;
    }
    
    uint64_t ReadDoubleword(uint64_t Vaddr)
    {
        return *(uint64_t *)getPaddr(Vaddr);
    }
    
    void WriteDoubleword(uint64_t Vaddr,uint64_t Doubleword)
    {
        *(uint64_t *)getPaddr(Vaddr)=Doubleword;
    }
};

class RegisterFile {
    uint64_t reg[32];
    public:
    uint64_t & operator [] (int i)
    {
        if(i==0)
        {
            reg[0]=0;
        }
        return reg[i];
    }
};

class instruction {
    public :
    uint32_t code;
    
    instruction(uint32_t code_) {code=code_;}
    uint32_t opcode() {return code & 0b1111111;}
    uint32_t rd() {return (code & 0b111110000000)>>7;}
    uint32_t func3() {return (code & 0b111000000000000)>>12;}
    uint32_t rs1() {return (code & 0b11111000000000000000)>>15;}
    uint32_t rs2() {return (code & 0b1111100000000000000000000)>>20;}
    uint32_t func7() {return (code & 0b11111110000000000000000000000000)>>25;}
    uint32_t shamt() {return rs2();}
    uint64_t imm_I(bool SignExt=true) {if(SignExt)return ((int64_t)((int)code)>>20);else return code>>20;}
    uint64_t imm_S(bool SignExt=true) {if(SignExt)return ((int64_t)(int)((((int)code)>>20)&(int)(~0b11111)|rd()));
                                            return (code>>20)&(~0b11111)|rd();}
    uint64_t imm_SB(bool SignExt=true)
    {
        if(SignExt)
            return (int64_t)(int)((((int)code)>>20)&(~0b11111)|(rd()&(~1))&(~(0b100000000000)|((code&0b10000000)<<4)));
        return (code>>20)&(~0b11111)|(rd()&(~1))&(~(0b100000000000)|((code&0b10000000)<<4));
    }

    uint64_t imm_U(bool SignExt=true) {if(SignExt)return int64_t(int(code &(~0b111111111111))); return code &(~0b111111111111);}
    uint64_t imm_UJ(bool SignExt=true)
    {
        if(SignExt)
            return (int64_t)(int)((((int)code)>>20&(~1)) & (~0b1111111110000000000) | (((code>>20)&1)<<11) | (code & 0b11111111000000000000));
        return (code>>20&(~1)) & (~0b1111111110000000000) | (((code>>20)&1)<<11) | (code & 0b11111111000000000000);
    }

};
    
RegisterFile x;
VirtualMemory mem;
uint64_t PC;
uint64_t & sp=x[2];
int main(int argc, char ** argv)
{
    bool verbose=true;
    ifstream fin (argv[1], ios::binary);
    Elf64_Ehdr Elfhdr;
    fin.read((char *)(&Elfhdr), sizeof(Elf64_Ehdr));
    Elf64_Phdr Prohdr;
    fin.read((char *)(&Prohdr), sizeof(Elf64_Phdr));
    //cout<<hex<<Prohdr.p_offset<<endl<<Prohdr.p_vaddr<<endl<<Prohdr.p_memsz<<endl;
    char * Content=new char [Prohdr.p_memsz];
    memset(Content, 0, Prohdr.p_memsz); 
    fin.seekg(Prohdr.p_offset);
    fin.read(Content, Prohdr.p_filesz);
    mem.load(Content, Prohdr.p_memsz, Prohdr.p_vaddr);
    sp=0xfefffb50;
    mem.load(new char [0x2000000], 0x2000000, 0xfe000000);
    PC=Elfhdr.e_entry;
    
    while(true)
    {
        instruction instr=mem.ReadWord(PC);
        if(verbose)printf("%08x:\t", instr.code);
        //cout<<dec<<(long)instr.imm_I()<<' '<<(long)instr.imm_SB()<<' '<<hex<<(long)instr.imm_UJ()<<endl;
        switch(instr.opcode())
        {
        	//64位零扩展加个false 
            case 0b0010111: //AUIPC
                if(verbose) printf("auipc\t%s,0x%lx", RegName[instr.rd()], instr.imm_U(true));
                x[instr.rd()] = instr.imm_U() + PC;
                break;
			
			case 0b0110111: //LUI
			    if(verbose) printf("lui\t%s,0x%lx", RegName[instr.rd()], instr.imm_U(true));
                    x[instr.rd()] = instr.imm_U();
				break;
			case 0b0000011: 
				switch(instr.func3())
				{
					case 0b000: //LB
						if(verbose) printf("lb\t%s,%s,0x%lx", RegName[instr.rd()], RegName[instr.rs1()], instr.imm_I(true));
						x[instr.rd()] = int64_t(int8_t(mem.ReadByte(instr.imm_I()+x[instr.rs1()])));
						break;
						
					case 0b001: //LH
						if(verbose) printf("lh\t%s,%s,0x%lx", RegName[instr.rd()], RegName[instr.rs1()], instr.imm_I(true));
						x[instr.rd()] = int64_t(int16_t(mem.ReadHalfword(instr.imm_I()+x[instr.rs1()])));
						break;
						
					case 0b010: //LW
						if(verbose) printf("lw\t%s,%s,0x%lx", RegName[instr.rd()], RegName[instr.rs1()], instr.imm_I(true));
						printf("1");
                        x[instr.rd()] = int64_t(int32_t(mem.ReadWord(instr.imm_I()+x[instr.rs1()])));
                        printf("1");
						break;
						
					case 0b011: //LBU
						if(verbose) printf("lbu\t%s,%s,0x%lx", RegName[instr.rd()], RegName[instr.rs1()], instr.imm_I(true));
						x[instr.rd()] = uint64_t(mem.ReadByte(instr.imm_I()+x[instr.rs1()]));
						break;
							
					case 0b100: //LHU
						if(verbose) printf("lhu\t%s,%s,0x%lx", RegName[instr.rd()], RegName[instr.rs1()], instr.imm_I(true));
						x[instr.rd()] = uint64_t(mem.ReadHalfword(instr.imm_I()+x[instr.rs1()]));
						break;		
				}
                break;
			case 0b0100011:
				switch(instr.func3())
				{
					case 0b000: //SB
						if(verbose) printf("sb\t%s,%s,0x%lx", RegName[instr.rs1()], RegName[instr.rs2()], instr.imm_S());
						mem.WriteByte(x[instr.rs1()] + instr.imm_S(), x[instr.rs2()]);
						break;
					case 0b001: //SH
						if(verbose) printf("sb\t%s,%s,0x%lx", RegName[instr.rs1()], RegName[instr.rs2()], instr.imm_S());
						mem.WriteHalfword(x[instr.rs1()] + instr.imm_S(), x[instr.rs2()]);
						break;
					case 0b010: //SW
						if(verbose) printf("sb\t%s,%s,0x%lx", RegName[instr.rs1()], RegName[instr.rs2()], instr.imm_S());
						mem.WriteWord(x[instr.rs1()] + instr.imm_S(), x[instr.rs2()]);
						break;
				}
                break;
			case 0b0110011:
				switch(instr.func3())
				{
					case 0b000: //ADD
						if(verbose) printf("add\t%s,%s,%s", RegName[instr.rd()], RegName[instr.rs1()], RegName[instr.rs2()]);
						x[instr.rd()] = x[instr.rs1()] + x[instr.rs2()];
						break;
						
					case 0b001: //SUB
						if(verbose) printf("sub\t%s,%s,%s", RegName[instr.rd()], RegName[instr.rs1()], RegName[instr.rs2()]);
						x[instr.rd()] = x[instr.rs1()] - x[instr.rs2()];
						break;
					
						
				}
                break;
				
				
            //default: Error("Invalid instruction\n");
        }
        PC+=4;
        cin.get();
    }
}

#include "l1_data_cache.h"
#include <iostream>
#include <string>
#include <fstream>
//#include <sstream>
#include <regex>

struct DEBUG_L2_memRsp :public pipe_reg_base, public L2_2_dcache_memRsp{
    DEBUG_L2_memRsp(){}

    void update_with(L2_2_dcache_memRsp memRsp){
    d_opcode = memRsp.d_opcode;
    d_source = memRsp.d_opcode;
    d_mask = memRsp.d_mask;
    d_data = memRsp.d_data;
    set_valid();
    }
};

class DEBUG_L2_model : public cache_building_block{
public:
    DEBUG_L2_model(){
        for(int i = 0;i<m_L2_data_array.size();++i){
            m_L2_data_array[i] = i*10;
        }
    }

    bool return_Q_is_empty(){
        return m_return_Q.empty();
    }

    L2_2_dcache_memRsp DEBUG_serial_pop(){
        assert(!m_return_Q.empty());
        auto value = m_return_Q.front();
        m_return_Q.pop_front();
        return value;
    }

    //非随机，一直提供次次尾部的元素
    L2_2_dcache_memRsp DEBUG_random_pop(){
        std::deque<L2_2_dcache_memRsp>::iterator value_iter;
        if (m_return_Q.size()<=2){
            value_iter = m_return_Q.begin();
        }else{
            value_iter = m_return_Q.begin() + 2;
        }
        auto value = *value_iter;
        m_return_Q.erase(value_iter);
        return value;
    }

    void DEBUG_L2_memReq_process(dcache_2_L2_memReq req, cycle_t time){
        if (!m_process_Q[m_minimal_process_latency-1].is_valid()){
            enum TL_UH_D_opcode return_op;
            cache_line_t return_data{};
            std::array<bool,LINEWORDS> return_mask{};
            if(req.a_opcode == PutFullData || (req.a_opcode == PutPartialData && req.a_param == 0x0)){
                return_op = AccessAck;//常规写入确实、替换写回、冲刷
            }else{
                return_op = AccessAckData;
                if (req.a_opcode == Get){//常规读出缺失
                    if(req.a_param == 0x0){
                        return_mask.fill(true);
                        assert((req.a_address + LINEWORDS < m_L2_capacity) && "L1 memReq Get请求的缓存行地址超出受测L2模型范围");
                        auto it_base = m_L2_data_array.begin()+req.a_address;
                        std::copy(it_base,it_base+LINEWORDS,return_data.begin());
                    }else{//LR
                        assert((req.a_param == 0x1) && "非法Get，在L2被逮捕");
                        return_mask[0] = true;
                        return_data[0] = m_L2_data_array[req.a_address];
                    }
                }else if(req.a_opcode == PutPartialData && req.a_param == 0x1){//SC
                    return_mask[0] = true;
                    return_data[0] = 0x0;//0x0表示成功，0x1表示失败
                }else{
                    assert(true && "L2处理到非法memReq请求");
                }
            }
            L2_2_dcache_memRsp new_memRsp = L2_2_dcache_memRsp(return_op,req.a_source,return_mask,return_data);
            m_process_Q[m_minimal_process_latency-1].update_with(new_memRsp);

            //debug info
            std::cout << std::setw(5) << time << " | memReq";
            std::cout << ", TLopcode=" << req.a_opcode <<std::endl;
        }
    }

    void cycle(){
        if (m_process_Q[0].is_valid()){
            m_return_Q.push_back(m_process_Q[0]);
        }
        for (unsigned stage = 0; stage < m_minimal_process_latency - 1; ++stage){
            m_process_Q[stage] = m_process_Q[stage + 1];
            m_process_Q[stage + 1].invalidate();
        }
    }
private:
    //从L2 memReq到L2 memRsp的最小间隔周期
    constexpr static int m_minimal_process_latency = 3;
    constexpr static int m_L2_capacity = 64;
    std::array<DEBUG_L2_memRsp,m_minimal_process_latency> m_process_Q {};
    std::deque<L2_2_dcache_memRsp> m_return_Q;
    std::array<u_int32_t,m_L2_capacity> m_L2_data_array;
};

class test_env : cache_building_block{

public:
    test_env(){
        //DEBUG_init_stimuli();
    }

    test_env(int verbose){
        verbose_level = verbose;
    }

    test_env(bool dump_csv){
        m_dump_csv = dump_csv;
    }

    test_env(int verbose, bool dump_csv){
        verbose_level = verbose;
        m_dump_csv = dump_csv;
    }

    void print_config_summary(){
        std::cout << "NLANE=" << NLANE << std::endl;
        std::cout << "NSET=" << NSET << "; NWAY=" << NWAY << std::endl;
        std::cout << "LINEWORDS=" << LINEWORDS << std::endl;
        std::cout << "N_MSHR_ENTRY=" << N_MSHR_ENTRY << std::endl;
        std::cout << "N_MSHR_SUBENTRY=" << N_MSHR_SUBENTRY << std::endl;
        std::cout << "N_MSHR_SPECIAL_ENTRY=" << N_MSHR_SPECIAL_ENTRY << std::endl;
        if(verbose_level>=4){
            std::cout << "CORE_RSP_Q_DEPTH=" << CORE_RSP_Q_DEPTH << std::endl;
            std::cout << "MEM_REQ_Q_DEPTH=" << MEM_REQ_Q_DEPTH << std::endl;
            std::cout << "MEM_RSP_Q_DEPTH=" << MEM_RSP_Q_DEPTH << std::endl;
        }
    }

    void DEBUG_print_coreRsp_pop(cycle_t time){
        if (dcache.m_coreRsp_Q.m_Q.size() != 0){
            dcache.m_coreRsp_Q.DEBUG_print(time);
            dcache.m_coreRsp_Q.m_Q.pop_front();
        }
        //dcache.m_coreRsp_ready = true;
    }

    void DEBUG_cycle(cycle_t time, std::ifstream& instr_file_name, std::ofstream& waveform_file){
        DEBUG_cycle(time,instr_file_name);
        DEBUG_waveform_a_cycle(waveform_file);
    }

    void DEBUG_cycle(cycle_t time, std::ifstream& instr_file_name){
        std::string instruction;

        DEBUG_print_coreRsp_pop(time);
        L2.cycle();
        if(!dcache.m_memReq_Q.is_empty()){
            L2.DEBUG_L2_memReq_process(dcache.m_memReq_Q.m_Q.front(), time);
            dcache.m_memReq_Q.m_Q.pop_front();
        }
        dcache.cycle(time);
        if(!L2.return_Q_is_empty()){
            dcache.m_memRsp_Q.m_Q.push_back(L2.DEBUG_serial_pop());
        }
        if(!dcache.m_coreReq.is_valid() && !instr_file_name.eof()){
            getline(instr_file_name, instruction);
            LSU_2_dcache_coreReq coreReq;
            if(parse_instruction(instruction,coreReq,time)){
                dcache.m_coreReq.update_with(coreReq);
            }//no else, just skip this cycle coreReq
        }
    }

    bool parse_instruction(std::string instruction, LSU_2_dcache_coreReq& coreReq, cycle_t time){
        std::regex field_regex("\\s*(\\S+)\\s+(\\S+)\\s*");
        //std::regex opcode_regex("([^\\.]+)(?:\\.(\\S+))*");
        //std::regex reg_imm_regex("(\\S+)(?:\\,(\\S+))*");
        std::smatch field_match;
        if (!regex_match(instruction, field_match, field_regex)) {
            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，空行或非法指令："<< instruction << std::endl;
            }
            return false;
        }
        if(verbose_level>=4){
            std::cout << "周期"<< time << "，opcode："<< field_match[1].str() << "。寄存器字段：" ;//<< field_match[2].str() << std::endl;
        }
        std::string opcode = field_match[1].str();
        /*std::vector<std::string> opcode_fields;
        std::string opcode_field_temp;
        std::stringstream opcode_ss(opcode);
        while(getline(opcode_ss, opcode_field_temp, '.')){
            opcode_fields.push_back(opcode_field_temp);
            if(verbose_level>=2){
                std::cout << opcode_fields.back() << " | " ;
            }
        }*/
        std::string reg_imm = field_match[2].str();
        std::array<std::string,4> reg_imm_fields;
        std::string reg_imm_temp;
        std::stringstream reg_imm_ss(reg_imm);
        int field_cnt=0;
        while(getline(reg_imm_ss, reg_imm_temp, ',')){
            reg_imm_fields[field_cnt]=reg_imm_temp;
            if(verbose_level>=4){
                std::cout << reg_imm_temp << " | " ;
            }
            ++field_cnt;
        }
        if(verbose_level>=4){std::cout << std::endl;}

        enum LSU_cache_coreReq_opcode coreReq_opcode;
        uint32_t coreReq_type;
        u_int32_t coreReq_wid;
        u_int32_t coreReq_id;
        u_int32_t coreReq_block_idx;
        vec_nlane_t p_addr{};
        std::array<bool,NLANE> p_mask{};
        p_mask.fill(false);
        vec_nlane_t p_data{};

        if(opcode == "lb" || opcode == "lh" || opcode == "lw" ||
        opcode == "lr.w" || opcode == "vle32.v"){
            coreReq_opcode = Read;
            if (opcode == "lr.w"){
                coreReq_type = 1;
            }else{
                coreReq_type = 0;
            }
            coreReq_wid = random(0,31);
            coreReq_id = cast_regidx_to_int(reg_imm_fields[0]);
            coreReq_block_idx = cast_addr_to_int(reg_imm_fields[1]);
            if(opcode == "lb" || opcode == "lh" || opcode == "lw" || opcode == "lr.w"){
                p_addr[0] = 0x0;
                p_mask[0] = true;
            }else{
                if(opcode == "vle32.v"){
                    for(int i = 0;i<NLANE;++i){
                        p_addr[i] = i;
                    }
                    p_mask.fill(true);
                }
            }
            
            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，Load, op = " << opcode << std::endl;
            }
        }else if(opcode == "sb" || opcode == "sh" || opcode == "sw" || 
        opcode == "sc.w" || opcode == "vse32.v" ){//|| opcode == "vsse32.v" || 
        //opcode == "vsuxe32.v" || opcode == "vsoxe32.v"){
            coreReq_opcode = Write;
            if (opcode == "sc.w"){
                coreReq_type = 1;
            }else{
                coreReq_type = 0;
            }
            coreReq_wid = random(0,31);
            if(opcode == "sc.w"){
                coreReq_id = cast_regidx_to_int(reg_imm_fields[0]);
                coreReq_block_idx = cast_addr_to_int(reg_imm_fields[2]);
                reg_imm_fields[0] = reg_imm_fields[1];//data
            }else{
                coreReq_id = -1;
                coreReq_block_idx = cast_addr_to_int(reg_imm_fields[1]);
            }
            
            if(opcode == "sb" || opcode == "sh" || opcode == "sw" || opcode == "sc.w"){
                p_addr[0] = 0x0;
                p_mask[0] = true;
                p_data[0] = std::stoi(reg_imm_fields[0]);
            }else{
                if(opcode == "vse32.v"){
                    u_int32_t data_base = std::stoi(reg_imm_fields[0]);
                    for(int i = 0;i<NLANE;++i){
                        p_addr[i] = i;
                        p_data[i] = data_base + i;//不通用，仅用于测试
                    }
                    p_mask.fill(true);
                }
            }

            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，Store, op = " << opcode << std::endl;
            }
        }else if(opcode == "fence"){
            std::string t0 = reg_imm_fields[0];
            std::string t1 = reg_imm_fields[1];
            coreReq_opcode = InvOrFlu;
            coreReq_wid = random(0,31);
            if ((t0=="r"&&t1=="r") || (t0=="r"&&t1=="rw") || (t0=="w"&&t1=="r") ||
            (t0=="w"&&t1=="rw") || (t0=="rw"&&t1=="r") || (t0=="rw"&&t1=="rw")){
                coreReq_type = 0x0;
            }else if((t0=="w"&&t1=="w") || (t0=="rw"&&t1=="w")){
                coreReq_type = 0x1;
            }else if((t0=="r"&&t1=="w")){
                coreReq_type = 0x2;
            }else{
                if(verbose_level>=2){
                    std::cout <<"周期"<< time << "，非法FENCE： " << instruction << std::endl;
                }
                return false;
            }

            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，Fence, op = " << opcode << std::endl;
            }
        }else if(opcode[0] == 'a' && opcode[1] == 'm' && opcode[2] == 'o'){
            coreReq_opcode = Amo;
            coreReq_type = amoswap;//不通用，仅用于测试
            coreReq_wid = random(0,31);
            coreReq_id = cast_regidx_to_int(reg_imm_fields[0]);
            coreReq_block_idx = cast_addr_to_int(reg_imm_fields[2]);
            p_addr[0] = 0x0;
            p_mask[0] = true;
            p_data[0] = std::stoi(reg_imm_fields[1]);

            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，AMO, op = " << opcode << std::endl;
            }
        }else{
            if(verbose_level>=2){
                std::cout <<"周期"<< time << "，非法指令："<< instruction << std::endl;
            }
            return false;
        }

        coreReq = LSU_2_dcache_coreReq(coreReq_opcode,
            coreReq_type,
            coreReq_wid,
            coreReq_id,
            coreReq_block_idx,
            p_addr,
            p_mask,
            p_data);
        return true;
    }

    //输入：寄存器索引
    int cast_regidx_to_int(std::string reg_idx){
        assert(reg_idx[0]=='x' || reg_idx[0]=='v');
        std::string number = reg_idx.substr(1);
        return std::stoi(number);
    }

    //输入：括号中的地址
    int cast_addr_to_int(std::string addr_in_bracket) {
        assert(addr_in_bracket.front() == '(' && addr_in_bracket.back() == ')');
        addr_in_bracket = addr_in_bracket.substr(1);
        addr_in_bracket = addr_in_bracket.substr(0, addr_in_bracket.size() - 1);
        return std::stoi(addr_in_bracket);
    }

    void DEBUG_waveform_title(std::ofstream& waveform){
        waveform << "cReq_v,cReq_op,cReq_type,cReq_wid,cReq_id,cReq_block_idx,cReq_block_offset_0,cReq_mask_0,cReq_mask_1,cReq_data_0" << "," ;
        waveform << "cRsp_v,cRsp_wid,cRsp_id,cRsp_mask_0,cRsp_wxd,cRsp_data_0" << ",";
        waveform << "d_v,d_op,d_source,d_mask_0,d_mask_1,d_data_0" << ",";
        waveform << "a_v,a_op,a_param,a_source,a_addr,a_mask_0,a_mask_1,a_data" << std::endl;
    }

    void DEBUG_waveform_a_cycle(std::ofstream& waveform){
        dcache.DEBUG_waveform_a_cycle(waveform);
        waveform << std::endl;
    }

    l1_data_cache dcache;
private:
    //std::deque<LSU_2_dcache_coreReq> coreReq_stimuli;
    DEBUG_L2_model L2;
    /*
    verbose_level = 0: 无任何打印信息
    verbose_level = 1: 重要打印信息
    verbose_level = 2: 主要
    */
    int verbose_level=1;
    bool m_dump_csv=true;
};

int main(int argc, char *argv[]) {

    if (argc < 2) { // 检查命令行参数是否正确
        std::cerr << "Usage: " << argv[0] << " input_file" << std::endl;
        return 1;
    }

    std::ifstream infile(argv[1]);
    if (!infile) { // 检查文件是否打开成功
        std::cerr << "Error: cannot open file " << argv[1] << std::endl;
        return 1;
    }

    std::cout << "modeling cache now" << std::endl;
    bool dump_csv = true;
    test_env tb(2,dump_csv);
    tb.print_config_summary();
    std::ofstream waveform("test_result.csv");
    if (dump_csv){
        tb.DEBUG_waveform_title(waveform);
    }
    //tb.dcache.m_tag_array.DEBUG_random_initialize(100);
    //tb.dcache.m_tag_array.DEBUG_visualize_array(28,4);

    std::cout << std::endl << " time | event" << std::endl;
    std::string instruction;
    LSU_2_dcache_coreReq coreReq;
    int i=0;
    /*while(!infile.eof()){
        getline(infile, instruction);
        tb.parse_instruction(instruction,coreReq,i);
        ++i;
    }*/
    for (int i = 100 ; i < 120 ; ++i){
        if (dump_csv){
            tb.DEBUG_cycle(i,infile,waveform);
        }else{
            tb.DEBUG_cycle(i,infile);
        }
    }
    
    tb.dcache.m_tag_array.DEBUG_visualize_array(28,4);

    waveform.close();
    infile.close();
    return 0;
}
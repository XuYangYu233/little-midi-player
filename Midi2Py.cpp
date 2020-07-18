#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<windows.h>
#include<math.h>
#include<mmsystem.h>
#include<thread>
#include<mutex>

#pragma comment(lib,"winmm.lib")

#define DTM(x) (x & 127) //用于处理delta time中的动态字节

long timeline=-1;       //时间轴
short yinyu_fixer=2;    //音域修正值，一般为2或1
float speed_fixer=0.97; //曲速修正系数
std::mutex mtx;         //线程锁。目的是减少CPU空转的时间，降低CPU使用率

/*模块划分：头部解析模块，单音轨解析模块，单音符播放模块，曲目播放模块*/
//头部信息传递用的结构体
typedef struct head_infomation{
    short num_of_soundtracks;
    int ticks_per_meter;
}HEAD_INFO;//文件头部信息结构体

typedef struct midi_event{
    long delta_time;
    unsigned char func_byte;
    unsigned char data_byte_a;
    unsigned char data_byte_b;
}MIDI_EVENT;//MIDI事件的结构体

typedef struct meta_event{
    long delta_time;
    unsigned char type_of_event;
    long length_of_data_area;
    unsigned char data_area[128];
}META_EVENT;//META事件的结构体，暂时用不上

typedef struct linklist{
    MIDI_EVENT mid_evn;
    struct linklist *next;
}LINKLIST;//链表的节点

typedef struct global_information{
    char name[64];
    int us_per_aquarter_meter;//每四分之一拍的微秒数
    unsigned char meter_info[4];
    unsigned char key_signature;//调号
    long lenth_of_music;
}GLOBAL_INFO;//全局信息，储存曲名，曲速，调号等


void sleep_us(float t);
float chufa(int beichushu,int chushu);
void play_sound(unsigned char note);
void decoding_func(unsigned char note,char *soundname);
void add_node(LINKLIST **end,MIDI_EVENT mid_evn);
long delta_time_manage(FILE *fi,unsigned int *bytes_next_to_read);
HEAD_INFO file_head_analysis(FILE *fi);
unsigned int track_head_analysis(FILE *fi);
LINKLIST *track_read(FILE *fi,unsigned int bytes_next_to_read,GLOBAL_INFO *glo_inf);
void track_play(LINKLIST *head,float us_per_tick);
void track_timer(float us_per_tick,long lenth_of_time);



//头部解析函数，返回值包含了文件头中的信息
HEAD_INFO file_head_analysis(FILE *fi){
    unsigned char *stra=NULL;
    HEAD_INFO result;

    stra = (unsigned char *)malloc(4);
    if(stra == NULL){
        printf("Malloc error!position: file_head_analysis 0x0001");
        system("pause");
        exit(0);
    }
    fread(stra,4,1,fi);
    if(!(stra[0]=='M'&&stra[1]=='T'&&stra[2]=='h'&&stra[3]=='d')){
        printf("Not a midi!\n");
        system("pause");
        exit(0);
    }

    fread(stra,4,1,fi);
    free(stra);

    stra = (unsigned char *)calloc(1,6);
    if(stra == NULL){
        printf("Calloc error!position: file_head_analysis 0x0002");
        system("pause");
        exit(0);
    }
    fread(stra,6,1,fi);
    if(stra[1] == 2){
        printf("Not mode 2! Un-identifiable\n");
        system("pause");
        exit(0);
    }
    result.num_of_soundtracks = stra[3];
    result.ticks_per_meter = stra[4] * 256 + stra[5];

    free(stra);

    return result;
}

//音轨头部解析函数，返回值为此音轨接下来需要读取的字节数
unsigned int track_head_analysis(FILE *fi){
    unsigned int bytes_next_to_read=0;
    unsigned char *stra;

    stra = (unsigned char *)calloc(4,1);
    if(stra == NULL){
        printf("Calloc error!position: track_head_analysis 0x0001");
        system("pause");
        exit(0);
    }
    fread(stra,1,4,fi);
    if(!(stra[0]=='M'&&stra[1]=='T'&&stra[2]=='r'&&stra[3]=='k')){
        printf("Sound track not found!\n");
        system("PAUSE");
        exit(0);
    }
    
    fread(stra,1,4,fi);
    bytes_next_to_read = stra[0]*256*256*256+stra[1]*256*256+stra[2]*256+stra[3];
    free(stra);

    return bytes_next_to_read;
}

//音轨读取模块，返回值为链表的头指针，也就是返回一个链表
LINKLIST *track_read(FILE *fi,unsigned int bytes_next_to_read,GLOBAL_INFO *glo_inf){
    LINKLIST *head,*node,*end;
    head = (LINKLIST *)malloc(sizeof(LINKLIST));
    if(head == NULL){
        printf("Malloc error!position: track_read 0x0001");
        system("pause");
        exit(0);
    }
    end = head;
    end->next = NULL;
    
    unsigned char last_func_byte=0;
    long sum_of_deltatime=0;
    while(1){
        MIDI_EVENT temp;
        memset(&temp,0,sizeof(MIDI_EVENT));
        
        temp.delta_time = delta_time_manage(fi,&bytes_next_to_read);
        sum_of_deltatime += temp.delta_time;
        temp.func_byte = fgetc(fi);
        bytes_next_to_read--;
        if(temp.func_byte >= 128){
            switch (temp.func_byte)
            {
            case 255/*0xff*/:
                temp.data_byte_a = fgetc(fi);
                temp.data_byte_b = fgetc(fi);
                bytes_next_to_read -= 2;

                if(temp.data_byte_a == 47/*0x2f*/&&temp.data_byte_b == 0){  //00 FF 2F 00的情况特殊，单独处理
                    add_node(&end,temp);
                    if(bytes_next_to_read != 0){
                        printf("reading error 0xff2f00,bytes_next_to_read=%d\n",bytes_next_to_read);
                        system("PAUSE");
                        exit(0);
                    }
                    goto label_EOFunction;  //使用goto直接跳至函数末尾
                }

                unsigned char *next_read;
                next_read = (unsigned char *)malloc(temp.data_byte_b);
                if(next_read == NULL){
                    printf("Malloc error!position: track_read 0x0002");
                    system("pause");
                    exit(0);
                }
                fread(next_read,1,temp.data_byte_b,fi); //读取后面的字节的信息存放在next_read中
                bytes_next_to_read -= temp.data_byte_b; 

                switch (temp.data_byte_a)
                {
                case 81/*0x51*/:

                    if(temp.data_byte_b != 3){
                        printf("reading error 0xff51\n");
                        system("pause");
                        exit(0);
                    }

                    if(glo_inf->us_per_aquarter_meter == 0){
                        glo_inf->us_per_aquarter_meter = next_read[0]*256*256 + next_read[1]*256 + next_read[2];
                    }

                    break;

                case 3/*0x03*/:
                case 8/*0x08*/:

                    if(!(glo_inf->name)[0]){
                        for(short i = 0;i < temp.data_byte_b;i++){
                            (glo_inf->name)[i] = next_read[i];
                        }
                        glo_inf->name[temp.data_byte_b] = '\0';
                    }

                    break;

                default:
                    /*啥都不干*/
                    break;
                }

                if(next_read != NULL){
                    free(next_read);
                }
                break;

            case 240/*0xf0*/:
                temp.data_byte_a = fgetc(fi);
                next_read = (unsigned char *)malloc(temp.data_byte_a);
                if(next_read == NULL){
                    printf("Malloc error!position: track_read 0x0003");
                    system("pause");
                    exit(0);
                }
                fread(next_read,1,temp.data_byte_a,fi);
                bytes_next_to_read -= temp.data_byte_a;

                //对于系统码啥也不做

                free(next_read);

                break;

            case 241:
            case 242:
            case 243:
            case 244:
            case 245:
            case 246:
            case 247:
            case 248:
            case 249:
            case 250:
            case 251:
            case 252:
            case 253:
            case 254:
                printf("undefined func_byte!\n");
                system("PAUSE");
                exit(0);

            default:
                if((temp.func_byte >= 128 && temp.func_byte < 192)||(temp.func_byte >= 224 && temp.func_byte < 240)){
                    temp.data_byte_a = fgetc(fi);
                    temp.data_byte_b = fgetc(fi);
                    bytes_next_to_read -= 2;
                }

                else{ //0xc0 <= temp.func_byte < 0xe0 的情况
                    temp.data_byte_a = fgetc(fi);
                    bytes_next_to_read --;
                }
                break;
            }
            add_node(&end,temp);
            last_func_byte = temp.func_byte;
        }
        else{
            if(last_func_byte < 128){
                printf("last byte reading error\n");
                system("PAUSE");
                exit(0);
            }
            if(last_func_byte >= 192 && last_func_byte < 224){ //CX和DX的情况
                temp.data_byte_a = temp.func_byte;
                temp.func_byte = last_func_byte;
            }
            else{
                temp.data_byte_a = temp.func_byte;
                temp.data_byte_b = fgetc(fi);
                bytes_next_to_read--;
                temp.func_byte = last_func_byte;
            }
            add_node(&end,temp);
        }
        //一个事件写入链表了接下来要干什么？啥都不用干
    }

    label_EOFunction:
    if(sum_of_deltatime > glo_inf->lenth_of_music){
        glo_inf->lenth_of_music = sum_of_deltatime;
    }
    if((glo_inf->name)[0]==0){
        strcpy(glo_inf->name,"Unknown");
    }
    end->next = NULL;
    return head;
}

//链表尾部添加节点
void add_node(LINKLIST **end,MIDI_EVENT mid_evn){
    LINKLIST *newnode;
    newnode = (LINKLIST *)malloc(sizeof(LINKLIST));
    if(newnode == NULL){
        printf("Malloc error!position: add_node 0x0001");
        system("pause");
        exit(0);
    }
    newnode->mid_evn = mid_evn;
    (*end)->next = newnode;
    newnode->next = NULL;
    *end = newnode;

    return;
}

//处理delta time的模块，返回值为十进制的delta time，同时bytes_next_to_read减去了相应读取的字符数
long delta_time_manage(FILE *fi,unsigned int *bytes_next_to_read){
    long delta_time=0;
    unsigned char *dt_bytes;
    int handled_bytes=0;
    dt_bytes = (unsigned char *)malloc(++handled_bytes);
    if(dt_bytes == NULL){
        printf("Malloc error!position: delta_time 0x0001\n");
        system("pause");
        exit(0);
    }
    while(1){
        dt_bytes[handled_bytes-1] = fgetc(fi);
        if(dt_bytes[handled_bytes-1] >= 128){
            dt_bytes = (unsigned char *)realloc(dt_bytes,++handled_bytes);
            if(dt_bytes == NULL){
                printf("Ralloc error!position: delta_time 0x0002\n");
                system("pause");
                exit(0);
            }
        }
        else{
            break;
        }
    }
    
    for(int i=handled_bytes;i>0;i--){
        delta_time += DTM(dt_bytes[i-1]) * (int)pow(128,(handled_bytes-i));
    }

    *bytes_next_to_read -= handled_bytes;

    return delta_time;
}

//播放声音
void play_sound(unsigned char note){
    char sound_name[5];    //音名
    char temp_command[128]={0};     //mciSendString的命令
    memset(sound_name,'\0',5);

    if(note==255){
        strcpy(sound_name,"No_sound");    //播放一段无声的声音，用于加载与播放器有关的DLL文件
    }
    else{
        decoding_func(note,sound_name);   //将键盘上的键对应地解码成音名
    }

    sprintf(temp_command,"open piano\\%s.mp3 alias %s",sound_name,sound_name);
    mciSendStringA(temp_command,0,0,0); //[4] //打开音名.mp3
    sprintf(temp_command,"play %s",sound_name);
    mciSendStringA(temp_command,0,0,0);    //播放
    Sleep(3000);
    sprintf(temp_command,"close %s",sound_name);   //关闭
    mciSendStringA(temp_command,0,0,0);
    return;
}

//解码音符代码
void decoding_func(unsigned char note,char *soundname){
    char yin_diao[][3]={"C","Cs","D","Ds","E","F","Fs","G","Gs","A","As","B"};
    char yin_jie[][2]={"0","1","2","3","4","5","6","7"};
    
    short yd_mark,yj_mark;
    yd_mark = note%12;
    yj_mark = (note/12)-yinyu_fixer;
    if((yj_mark >= 0 && yj_mark < 7) || (yj_mark == 7 && yd_mark == 0))
        strcpy(soundname,yin_diao[yd_mark]);
        strcat(soundname,yin_jie[yj_mark]);  //此处有数组越界风险

    return;
}

//高精度除法，使用python逃课
float chufa(int beichushu,int chushu){
    FILE *py_pipe;
    char py_string[128]={0};
    float result=0;
    float *p_res=NULL;
    p_res = (float *)malloc(sizeof(float));
    if(p_res == NULL){
        printf("Malloc error!position: chufa 0x0001");
        system("pause");
        exit(0);
    }

    sprintf(py_string,".\\Midi_read_try_py.py %d %d",beichushu,chushu);
    py_pipe = _popen(py_string,"r");

    memset(py_string,'\0',128);
    fscanf(py_pipe,"%f",p_res);
    result = *p_res;
    free(p_res);
    fclose(py_pipe);

    return result;
}

//根据链表播放音乐
void track_play(LINKLIST *head,float us_per_tic){
    //Sleep(Sleeping_time*1000);
    LINKLIST *cur=NULL;
    cur = head->next;

    long front_time=0;
    while(cur != NULL){
        while(1){
            mtx.lock();         //锁
            mtx.unlock();       //降低了判断的频率，减少CPU工作量
            if(cur->mid_evn.delta_time <= timeline - front_time){  //此处等于号可能需要改成小于等于号？
                if(cur->mid_evn.func_byte >= 144 && cur->mid_evn.func_byte < 160 && cur->mid_evn.data_byte_b != 0){
                    std::thread t(play_sound,cur->mid_evn.data_byte_a);
                    t.detach();
                }
                front_time = cur->mid_evn.delta_time + front_time;
                break;
            }
            else{
                continue;
            }
        }
        cur = cur->next;
        
    }
}

//延时函数，精确到微秒级，代替Sleep()
void sleep_us(float t){
    LARGE_INTEGER t1,t2,tc;
    double time=0;
    QueryPerformanceFrequency(&tc);
    QueryPerformanceCounter(&t1);
    while(1){
        QueryPerformanceCounter(&t2);
        time = (double)(t2.QuadPart - t1.QuadPart)*1000*1000/(double)tc.QuadPart;
        if(time >= (double)t){
            break;
        }
    }
}

//计时专用函数，每一个tick就使timeline加一
void track_timer(float us_per_tick,long lenth_of_time){
    register float t;
    t = us_per_tick * speed_fixer;

    while(timeline <= lenth_of_time){
        mtx.lock();
        sleep_us(t*0.8);
        mtx.unlock();
        timeline++;
        sleep_us(t*0.2);

    }
    printf("Timer end\n");
    return;
}

int Play_midi(char *name){
    printf("MIDI playment try\n");

    FILE *fi=NULL;
    HEAD_INFO head_info;
    GLOBAL_INFO glo_inf;
    memset(&glo_inf,0,sizeof(GLOBAL_INFO));
    unsigned int bytes_that_next_to_read=0;
    short num_of_loop=0;
    float us_per_tick;
    char path[128]="midi_try\\";   //
    char midi_name[128];

    strcat(path, name);

    printf("Opening %s\n",path);
    fi = fopen(path,"rb");
    if(fi == NULL){
        printf("file open error\n");
        system("PAUSE");
        exit(0);
    }
    printf("File opened successfully\n");

    head_info = file_head_analysis(fi);
    printf("File head part loaded successfully\n");

    LINKLIST *ptr_array[16];
    for(num_of_loop=0;num_of_loop<16;num_of_loop++){
        bytes_that_next_to_read = track_head_analysis(fi);
        ptr_array[num_of_loop] = track_read(fi,bytes_that_next_to_read,&glo_inf);
        unsigned char eof_flag=0;
        eof_flag = fgetc(fi);
        if(feof(fi)){
            break;
        }
        else{
            fseek(fi,-1,SEEK_CUR);
        }
    }

    if(num_of_loop+1 != head_info.num_of_soundtracks){
        printf("number of soundtracks reading error!\n");
        system("PAUSE");
        exit(0);
    }
    fclose(fi);
    printf("Soundtracks loaded successfully\n");

    for(short i=0;i<num_of_loop+1;i++){
        //printf("this is track %hd:\n",i+1);
        LINKLIST *p;
        p = ptr_array[i]->next;
        while(p != NULL){
            if(p->mid_evn.func_byte>=144&&p->mid_evn.func_byte<160){
                if(p->mid_evn.data_byte_a<12*(yinyu_fixer)||p->mid_evn.data_byte_a>12*(yinyu_fixer+7)){ //12，96||24，108
                    printf("note out of range!\n");
                }
            }
            //printf("delta time is %4ld,func byte is %02X,byte a and b are %02X and %02X\n",p->mid_evn.delta_time,p->mid_evn.func_byte,p->mid_evn.data_byte_a,p->mid_evn.data_byte_b);
            p = p->next;
        }
    }
    printf("Soundtracks checked successfully, ready to play\n");

    us_per_tick = (double)glo_inf.us_per_aquarter_meter / (double)head_info.ticks_per_meter;
    printf("Number of soundtracks is %hd \nTicks per meter are %d\n",head_info.num_of_soundtracks,head_info.ticks_per_meter);
    printf("Music name is %s \nus per aquarter meter are %d\n",glo_inf.name,glo_inf.us_per_aquarter_meter);
    printf("us per tick is %f\n",us_per_tick);
    printf("Lenth of music is %ld ticks, which eqlus to %ld s\n",glo_inf.lenth_of_music,1+(long)(glo_inf.lenth_of_music*us_per_tick/1000000));
    printf("Basic information listed successfully, starting in 1000 ms\n");
    
    std::thread t1(play_sound,255);
    t1.detach();

    for(short i=0;i<num_of_loop+1;i++){
        std::thread t(track_play,ptr_array[i],us_per_tick);
        t.detach();
        printf("Sound track %hd loaded successfully\n",i+1);
    }
    Sleep(1000);
    std::thread timer(track_timer,us_per_tick,glo_inf.lenth_of_music);
    printf("Timer track loaded successfully\n");
    timer.join();

    //新方案：规定一个线程专门用来计时，其他的存储了音轨信息的线程以这个线程为标准来播放对应的音，而不在自己的线程中计时
    // 保证了时间轴的一致性，虽然不能保证与现实时间的绝对对应，但保证了整支乐曲中的时间的相对对应
    // 先尝试用thread中的互斥锁试一下，不行再用pthread.h,     甚至可以不需要互斥锁？
    // 尝试在读取midi文件的时候获取音乐时长

    Sleep(3000);
    printf("End\n");
    //system("PAUSE");
    return 0;
}

int main(int argc, char* argv[]){
    yinyu_fixer = atoi(argv[2]);
    Play_midi(argv[1]);

    return 0;
}
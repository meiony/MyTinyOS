;主引导程序
;------------------------------------------------------------------------------
%include "boot.inc"		;让编译器在编译之前,把boot.inc文件包含进来
SECTION MBR vstart=0x7c00
	mov ax,cs
	mov ds,ax
	mov es,ax
	mov ss,ax
	mov fs,ax
	mov sp,0x7c00
	mov ax,0xb800
	mov gs,ax

;清屏
;利用0x06功能,上卷所有行,则可清屏
;-------------------------------------------------------------------------------
;INT 0x10 功能号:0x06 功能描述:上卷窗口
;-------------------------------------------------------------------------------
;输入;
;AH 功能号:0x06
;AL = 上卷的行数(如果为0,表示全部)
;BH = 上卷行属性
;(CL,CH) = 窗口左上角(X,Y)位置
;(DL,DH) = 窗口右下角的(X,Y)位置
;无返回值
	mov ax,0600h
	mov bx,0700h
	mov cx,0	;左上角:(0,0)
	mov dx,184fh	;右下角:(80,25)
;因为VGA文本模式中,一行只能容纳80个字符,共25行
	; 下标从0开始,所有0x18=24,0x4f=79
	int 10h		;int 10h

	;输出字符串MBR
	mov byte [gs:0x00],'1'
	mov byte [gs:0x01],0xA4

	mov byte [gs:0x02],' '
	mov byte [gs:0x03],0xA4

	mov byte [gs:0x04],'M'
	mov byte [gs:0x05],0xA4		;A表示绿色背景闪烁,4表示前景颜色为红色

	mov byte [gs:0x06],'B'
	mov byte [gs:0x07],0xA4

	mov byte [gs:0x08],'R'
	mov byte [gs:0x09],0xA4

	mov eax,LOADER_START_SECTOR	;起始扇区lba地址,0x2
	mov bx,LOADER_BASE_ADDR		;写入的地址,0x900
	mov cx,4			;待写入的扇区数,由于loader.bin超过了512个字节，可能是多个扇区
	call rd_disk_m_16		;以下读取程序的起始部分(一个扇区)

	jmp LOADER_BASE_ADDR+0x300

;-------------------------------------------------------------------------------------
;功能:读取硬盘n个扇区
rd_disk_m_16:
;-------------------------------------------------------------------------------------
					;eax=LBA扇区号
					;bx=将数据写入的内存地址
					;cx=读入的扇区数
	mov esi,eax			;备份eax,因为al在out命令中会使用,会影响到eax的低8位
	mov di,cx			;备份cx,cx在读数据的时候会使用到
;读写硬盘
;第一步:设置要读取的扇区数
	mov dx,0x1f2			;虚拟硬盘属于ata0,是Primary通道,所以sector count 是由0x1f2访问
	mov al,cl			;cl是cx的低8位,就读一个扇区,这样就能传过去了
	out dx,al			;读取的扇区数,sector count 记录要读取的扇盘数量

	mov eax,esi			;恢复eax,现在eax存的是其实扇区lba的地址,0x2,第二个扇区

;第二步:将LBA地址存入 0x1f3 ~ 0x1f6

	;LBA地址 7~0 位写入端口 0x1f3
	mov dx,0x1f3			;LBA low
	out dx,al			;eax的第8位,就是al

	;LBA地址 15~8 位写入端口 0x1f4
	mov cl,8
	shr eax,cl			;eax右移8位,让al的数,变为eax中8位
	mov dx,0x1f4			;LBA mid
	out dx,al

	;LBA地址 23~16 位写入端口 0x1f5
	shr eax,cl			;再右移8位
	mov dx,0x1f5			;LBA high
	out dx,al

	shr eax,cl			;这样al为0000
	and al,0x0f			;lba第24~27位
	or al,0xe0			;设置7~4位为1110,表示lba模式
	mov dx,0x1f6			;就是拼凑出device寄存器的值
	out dx,al

;第3步:向0x1f7端口写入读命令,0x20
	mov dx,0x1f7
	mov al,0x20
	out dx,al			;command:0x1f7,写入命令,写入的命令是读命令

;第四步:检测硬盘状态
 .not_ready:
	;同一端口,写时表示写入命令字,读时表示写入硬盘的状态,所以不需要更换dx的值
	nop				;减少打扰硬盘的工作
	in al,dx			;将Status的寄存器的值读入到al中
	and al,0x88			;第四位为1表示硬盘控制器已准备好数据传输,第七位为1表示硬盘忙,保存第4位和第7位
	cmp al,0x08			;若第4位为1,表示数据已经准备好了,若第7位为1,表示硬盘处于忙
	jnz .not_ready			;若未准备好,继续等,判断结果是否为0

;第5步,从0x1f0端口读数据
	mov ax,di			;这个时候di存的是上面备份的cx,及时要读取的扇区的数量
	mov dx,256			;每次in操作只读取两个字节,根据读入的数据总量(扇区数*512字节)
	mul dx				;dx*ax就是总数量/2,然后将值送到cx中,cx就是要in的次数
	mov cx,ax			;di为要读取的扇区数,一个扇区有512个字节,每次读入一个字,共需要di*512/2次,所以di*256

	mov dx,0x1f0
 .go_on_read:
	in ax,dx			;读入到ax中
	mov [bx],ax			;读入到bx指向的内存
	add bx,2			;每次读入2个字节
	loop .go_on_read		;cx是循环的次数
	ret

	times 510-($-$$) db 0
	db 0x55,0xaa

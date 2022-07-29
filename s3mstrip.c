#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>

#define mem16 ((uint16_t*)mem)
#define memhdr ((mhdr*)mem)

typedef struct {
	char title[28];
	uint16_t sig; // 0x101A
	uint16_t rsv0;
	uint16_t num_orders;
	uint16_t num_inst;
	uint16_t num_pat;
	uint16_t flags;
	uint16_t tracker;
	uint16_t sampletype;
	uint32_t SCRM;
	uint8_t global_volume;
	uint8_t init_speed;
	uint8_t init_tempo;
	uint8_t master_volume;
	uint8_t gus_click;
	uint8_t pan;
	uint8_t rsv1[8];
	uint16_t ext;
	uint8_t chn_settings[32];
} __attribute__((packed)) mhdr;

typedef struct {
	uint8_t type;
	uint8_t name[12];
} __attribute__((packed)) inst_stub;

typedef struct {
	uint8_t type;
	uint8_t name[12];
	uint8_t ptr_h;
	uint16_t ptr_l;
	uint32_t len;
	uint32_t lpstart;
	uint32_t lpend;
	uint8_t volume;
	uint8_t rsv0;
	uint8_t pack;
	uint8_t flags;
	uint32_t c2spd;
	uint8_t rsv1[12];
	uint8_t title[28];
	uint32_t SCRS;
} __attribute__((packed)) inst_pcm;

typedef struct {
	uint8_t type;
	uint8_t name[12];
	uint8_t rsv0[3];
	uint8_t opl[12];
	uint8_t volume;
	uint8_t unknown_i_fuckin_guess;
	uint16_t rsv1;
	uint32_t c2spd;
	uint8_t rsv2[12];
	uint8_t title[28];
	uint32_t SCRI;
} __attribute__((packed)) inst_adl;

int main(int argc, char** argv) {
	if(argc ^ 3) return printf("need [in] [out]\n");
	int ifd, ofd;
	if(
		((ifd = open(argv[1], O_RDWR)) < 0) |
		((ofd = open(argv[2], O_RDWR|O_CREAT, 0644)) < 0)
	) {
		f: return printf("error opening files (%i %i)\n", ifd, ofd);
	}
	unsigned long hdat = 0;
	struct stat s;
	fstat(ifd, &s);
	uint8_t* mem;
	if((mem = mmap(0, s.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, ifd, 0)) == MAP_FAILED) goto f;

	if(memhdr->flags & 0x80) hdat = ((int)memhdr->ext) << 4;
	uint16_t* t = (uint16_t*)(mem + sizeof(mhdr) + memhdr->num_orders);

	for(int i = 0; i < memhdr->num_inst; i++) {
		uint32_t a = ((int)*(t++)) << 4;
		printf("sample at offset %X\n", a);
		inst_stub* ca = mem + a;
		if(!(ca->type & (~7))) {
			if(!ca->type && a+sizeof(inst_stub) > hdat) hdat = a+sizeof(inst_stub);
			else if(!(ca->type^1)) {
				inst_pcm* p = ca;
				if(a+sizeof(inst_pcm) > hdat) hdat = a+sizeof(inst_pcm);
				long conptr = p->ptr_h;
				conptr = (((conptr << 16) | p->ptr_l) << 4);
				printf("calculated conptr: %lX\n", conptr);
				conptr += ((p->len << ((p->flags & 2) >> 1)) << ((p->flags & 4) >> 2));
				printf("calculated sz: %lX\n", ((p->len << ((p->flags & 2) >> 1)) << ((p->flags & 4) >> 2)));
				if(conptr > hdat) hdat = conptr;
			} else if(a+sizeof(inst_adl) > hdat) hdat = a+sizeof(inst_adl);
		}
	}

	for(int i = 0; i < memhdr->num_pat; i++) {
		uint32_t a = ((int)*(t++)) << 4;
		printf("pat at %X\n", a);
		if(*((uint16_t*)(mem + a)) + a > hdat) hdat = *((uint16_t*)(mem + a)) + a;
		printf("pat sz %hX\n", *((uint16_t*)(mem + a)));
	}

	write(ofd, mem, hdat);
	ftruncate(ofd, hdat);
	munmap(mem, s.st_size);
	close(ifd);
	close(ofd);
}

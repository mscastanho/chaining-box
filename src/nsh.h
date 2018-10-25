#ifndef NSH_H_
#define NSH_H_

// #include <stdint.h>

#define ETH_P_NSH              0x894F
#define NSH_HLEN_NO_META            8
#define NSH_BASE_HEADER_LEN         4
#define NSH_SPI_LEN                 3
#define NSH_SPH_LEN                 4
#define NSH_SERVICE_PATH_ID_LEN     3
#define NSH_SERVICE_INDEX_LEN       1

#define NSH_OAM_PACKET              0x2000
#define NSH_TTL_MASK                0x0FC0
#define NSH_TTL_DEFAULT             0x0FC0

#define NSH_LENGTH_MD_TYPE_1        0x0006
#define NSH_BASE_LENGHT_MD_TYPE_2   0x0002

#define NSH_MD_TYPE_MASK    0x00000F00
#define NSH_MD_TYPE_0       0x00
#define NSH_MD_TYPE_1       0x01
#define NSH_MD_TYPE_2       0x02
#define NSH_MD_TYPE_TEST    0x0F

#define NSH_SPI_MASK        0xFFFFFF00
#define NSH_SI_MASK         0x000000FF

#define NSH_NEXT_PROTO_IPV4  0x01
#define NSH_NEXT_PROTO_IPV6  0x02
#define NSH_NEXT_PROTO_ETHER 0x03
#define NSH_NEXT_PROTO_NSH   0x04
#define NSH_NEXT_PROTO_MPLS  0x05
#define NSH_NEXT_PROTO_EXP1  0xFE
#define NSH_NEXT_PROTO_EXP2  0xFF

struct nsh_hdr {
    __u16 basic_info; /* Ver, OAM bit, Unused and TTL */
    __u8 md_type;
    __u8 next_proto;
    __u32 serv_path;
} __attribute__((__packed__));

struct nsh_spi {
    __u8 spi_bytes[NSH_SPI_LEN];
} __attribute__((__packed__));

#endif

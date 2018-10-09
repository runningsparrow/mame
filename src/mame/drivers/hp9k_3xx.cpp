// license:BSD-3-Clause
// copyright-holders:R. Belmont
/***************************************************************************

  hp9k3xx.c: preliminary driver for HP9000 300 Series (aka HP9000/3xx)
  By R. Belmont

  TODO: Add DIO/DIO-II slot capability and modularize the video cards

  Currently supporting:

  310:
      MC68010 CPU @ 10 MHz
      HP custom MMU

  320:
      MC68020 CPU @ 16.67 MHz
      HP custom MMU
      MC68881 FPU

  330:
      MC68020 CPU @ 16.67 MHz
      MC68851 MMU
      MC68881 FPU

  340:
      MC68030 CPU @ 16.67 MHz w/built-in MMU
      MC68881 FPU

  370:
      MC68030 CPU @ 33 MHz w/built-in MMU
      MC68881 FPU

  380:
    MC68040 CPU @ 25 MHz w/built-in MMU and FPU

  382:
    MC68040 CPU @ 25? MHz w/built-in MMU and FPU
    Built-in VGA compatible video

  All models have an MC6840 PIT on IRQ6 clocked at 250 kHz.

  TODO:
    BBCADDR   0x420000
    RTC_DATA: 0x420001
    RTC_CMD:  0x420003
    HIL:      0x428000
    HPIB:     0x470000
    KBDNMIST: 0x478005
    DMA:      0x500000
    FRAMEBUF: 0x560000

    6840:     0x5F8001/3/5/7/9, IRQ 6

****************************************************************************/

#include "emu.h"
#include "logmacro.h"
#include "cpu/m68000/m68000.h"
#include "machine/6840ptm.h"
#include "bus/hp_dio/hp_dio.h"

#include "screen.h"
#include "softlist_dev.h"

#define MAINCPU_TAG "maincpu"
#define PTM6840_TAG "ptm"

class hp9k3xx_state : public driver_device
{
public:
	hp9k3xx_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_maincpu(*this, MAINCPU_TAG),
		m_vram16(*this, "vram16"),
		m_vram(*this, "vram")
	{ }

	void hp9k370(machine_config &config);
	void hp9k330(machine_config &config);
	void hp9k382(machine_config &config);
	void hp9k310(machine_config &config);
	void hp9k340(machine_config &config);
	void hp9k380(machine_config &config);
	void hp9k320(machine_config &config);
	void hp9k332(machine_config &config);
	void hp9k300(machine_config &config);

private:
	required_device<m68000_base_device> m_maincpu;
	virtual void machine_reset() override;

	optional_shared_ptr<uint16_t> m_vram16;
	optional_shared_ptr<uint32_t> m_vram;

	uint32_t screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);
	uint32_t hp_medres_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect);

	DECLARE_READ16_MEMBER(buserror16_r);
	DECLARE_WRITE16_MEMBER(buserror16_w);
	DECLARE_READ32_MEMBER(buserror_r);
	DECLARE_WRITE32_MEMBER(buserror_w);

	DECLARE_WRITE32_MEMBER(led_w)
	{
		if (mem_mask != 0x000000ff)
		{
			return;
		}
#if 0
		printf("LED: %02x  (", data&0xff);
		for (int i = 7; i >= 0; i--)
		{
			if (data & (1 << i))
			{
				printf("o");
			}
			else
			{
				printf("*");
			}
		}
		printf(")\n");
#endif
	}

	void hp9k310_map(address_map &map);
	void hp9k320_map(address_map &map);
	void hp9k330_map(address_map &map);
	void hp9k332_map(address_map &map);
	void hp9k370_map(address_map &map);
	void hp9k380_map(address_map &map);
	void hp9k382_map(address_map &map);
	void hp9k3xx_common(address_map &map);

        DECLARE_WRITE_LINE_MEMBER(dio_irq1_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_1, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq2_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_2, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq3_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_3, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq4_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_4, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq5_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_5, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq6_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_6, state, M68K_INT_ACK_AUTOVECTOR); };
        DECLARE_WRITE_LINE_MEMBER(dio_irq7_w) { m_maincpu->set_input_line_and_vector(M68K_IRQ_7, state, M68K_INT_ACK_AUTOVECTOR); };

	DECLARE_WRITE_LINE_MEMBER(cpu_reset);

	uint32_t m_lastpc;
};

uint32_t hp9k3xx_state::hp_medres_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	uint32_t *scanline;
	int x, y;
	uint32_t pixels;
	uint32_t m_palette[2] = { 0x00000000, 0xffffffff };

	for (y = 0; y < 390; y++)
	{
		scanline = &bitmap.pix32(y);
		for (x = 0; x < 512/4; x++)
		{
			pixels = m_vram[(y * 256) + x];

			*scanline++ = m_palette[(pixels>>24) & 1];
			*scanline++ = m_palette[(pixels>>16) & 1];
			*scanline++ = m_palette[(pixels>>8) & 1];
			*scanline++ = m_palette[(pixels & 1)];
		}
	}

	return 0;
}

// shared mappings for all 9000/3xx systems
void hp9k3xx_state::hp9k3xx_common(address_map &map)
{
	map(0x00000000, 0xffffffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0x00000000, 0x0001ffff).rom().region("maincpu", 0).w(FUNC(hp9k3xx_state::led_w));  // writes to 1fffc are the LED

	map(0x005f8000, 0x005f800f).rw(PTM6840_TAG, FUNC(ptm6840_device::read), FUNC(ptm6840_device::write)).umask32(0x00ff00ff);

	map(0x005f4000, 0x005f400f).ram(); // somehow coprocessor related - bootrom crashes if not present
}

// 9000/310 - has onboard video that the graphics card used in other 3xxes conflicts with
void hp9k3xx_state::hp9k310_map(address_map &map)
{
	map(0x000000, 0x01ffff).rom().region("maincpu", 0).nopw();  // writes to 1fffc are the LED

	map(0x510000, 0x510003).rw(FUNC(hp9k3xx_state::buserror16_r), FUNC(hp9k3xx_state::buserror16_w));   // no "Alpha display"
	map(0x538000, 0x538003).rw(FUNC(hp9k3xx_state::buserror16_r), FUNC(hp9k3xx_state::buserror16_w));   // no "Graphics"
	map(0x5c0000, 0x5c0003).rw(FUNC(hp9k3xx_state::buserror16_r), FUNC(hp9k3xx_state::buserror16_w));   // no add-on FP coprocessor

	map(0x5f8000, 0x5f800f).rw(PTM6840_TAG, FUNC(ptm6840_device::read), FUNC(ptm6840_device::write)).umask16(0x00ff);
	map(0x600000, 0x7fffff).rw(FUNC(hp9k3xx_state::buserror16_r), FUNC(hp9k3xx_state::buserror16_w));   // prevent reading invalid DIO slots
	map(0x800000, 0xffffff).ram();
}

// 9000/320
void hp9k3xx_state::hp9k320_map(address_map &map)
{
	hp9k3xx_common(map);

	// unknown, but bootrom crashes without
	map(0x00510000, 0x00510fff).ram();
	map(0x00516000, 0x00516fff).ram();
	map(0x00440000, 0x0044ffff).ram();

	// main memory
	map(0xfff00000, 0xffffffff).ram();
}

// 9000/330 and 9000/340
void hp9k3xx_state::hp9k330_map(address_map &map)
{
	hp9k3xx_common(map);

	map(0xffb00000, 0xffbfffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0xffc00000, 0xffffffff).ram();
}

// 9000/332, with built-in medium-res video
void hp9k3xx_state::hp9k332_map(address_map &map)
{
	hp9k3xx_common(map);

	map(0x00200000, 0x002fffff).ram().share("vram");    // 98544 mono framebuffer
	map(0x00560000, 0x00563fff).rom().region("graphics", 0x0000);   // 98544 mono ROM

	map(0xffb00000, 0xffbfffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0xffc00000, 0xffffffff).ram();
}

// 9000/370 - 8 MB RAM standard
void hp9k3xx_state::hp9k370_map(address_map &map)
{
	hp9k3xx_common(map);

	map(0xff700000, 0xff7fffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0xff800000, 0xffffffff).ram();
}

// 9000/380 - '040
void hp9k3xx_state::hp9k380_map(address_map &map)
{
	hp9k3xx_common(map);

	map(0x0051a000, 0x0051afff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));   // no "Alpha display"

	map(0xc0000000, 0xff7fffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0xff800000, 0xffffffff).ram();
}

// 9000/382 - onboard VGA compatible video (where?)
void hp9k3xx_state::hp9k382_map(address_map &map)
{
	hp9k3xx_common(map);

	map(0xffb00000, 0xffbfffff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));
	map(0xffc00000, 0xffffffff).ram();

	map(0x0051a000, 0x0051afff).rw(FUNC(hp9k3xx_state::buserror_r), FUNC(hp9k3xx_state::buserror_w));   // no "Alpha display"
}

uint32_t hp9k3xx_state::screen_update(screen_device &screen, bitmap_rgb32 &bitmap, const rectangle &cliprect)
{
	return 0;
}

/* Input ports */
static INPUT_PORTS_START( hp9k330 )
INPUT_PORTS_END

WRITE_LINE_MEMBER(hp9k3xx_state::cpu_reset)
{
}


void hp9k3xx_state::machine_reset()
{
	m_maincpu->set_reset_callback(write_line_delegate(FUNC(hp9k3xx_state::cpu_reset), this));
}

READ16_MEMBER(hp9k3xx_state::buserror16_r)
{
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
	m_lastpc = m_maincpu->pc();
	return 0;
}

WRITE16_MEMBER(hp9k3xx_state::buserror16_w)
{
	if (m_lastpc == m_maincpu->pc()) {
		logerror("%s: ignoring r-m-w double bus error\n", __FUNCTION__);
		return;
	}

	m_maincpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
}

READ32_MEMBER(hp9k3xx_state::buserror_r)
{
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
	m_lastpc = m_maincpu->pc();
	return 0;
}

WRITE32_MEMBER(hp9k3xx_state::buserror_w)
{
	if (m_lastpc == m_maincpu->pc()) {
		logerror("%s: ignoring r-m-w double bus error\n", __FUNCTION__);
		return;
	}
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, ASSERT_LINE);
	m_maincpu->set_input_line(M68K_LINE_BUSERROR, CLEAR_LINE);
}

MACHINE_CONFIG_START(hp9k3xx_state::hp9k300)
	ptm6840_device &ptm(PTM6840(config, PTM6840_TAG, 250000)); // from oscillator module next to the 6840
	ptm.set_external_clocks(250000.0f, 0.0f, 250000.0f);
	ptm.o3_callback().set(PTM6840_TAG, FUNC(ptm6840_device::set_c2));
	ptm.irq_callback().set_inputline("maincpu", M68K_IRQ_6);

	MCFG_SOFTWARE_LIST_ADD("flop_list", "hp9k3xx_flop")
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k310)
	hp9k300(config);
	MCFG_DEVICE_ADD(m_maincpu, M68010, 10000000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k310_map)

	bus::hp_dio::dio16_device &dio16(DIO16(config, "diobus", 0));
	dio16.set_cputag(m_maincpu);

	dio16.irq1_out_cb().set(FUNC(hp9k3xx_state::dio_irq1_w));
	dio16.irq2_out_cb().set(FUNC(hp9k3xx_state::dio_irq2_w));
	dio16.irq3_out_cb().set(FUNC(hp9k3xx_state::dio_irq3_w));
	dio16.irq4_out_cb().set(FUNC(hp9k3xx_state::dio_irq4_w));
	dio16.irq5_out_cb().set(FUNC(hp9k3xx_state::dio_irq5_w));
	dio16.irq6_out_cb().set(FUNC(hp9k3xx_state::dio_irq6_w));
	dio16.irq7_out_cb().set(FUNC(hp9k3xx_state::dio_irq7_w));

	DIO16_SLOT(config, "sl1", 0, "diobus", dio16_cards, "98544", false);
	DIO16_SLOT(config, "sl2", 0, "diobus", dio16_cards, "98603b", false);
	DIO16_SLOT(config, "sl3", 0, "diobus", dio16_cards, "98644", false);
	DIO16_SLOT(config, "sl4", 0, "diobus", dio16_cards, nullptr, false);

MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k320)
	MCFG_DEVICE_ADD(m_maincpu, M68020FPU, 16670000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k320_map)

	hp9k300(config);

	bus::hp_dio::dio16_device &dio32(DIO32(config, "diobus", 0));
	dio32.set_cputag(m_maincpu);

	dio32.irq1_out_cb().set(FUNC(hp9k3xx_state::dio_irq1_w));
	dio32.irq2_out_cb().set(FUNC(hp9k3xx_state::dio_irq2_w));
	dio32.irq3_out_cb().set(FUNC(hp9k3xx_state::dio_irq3_w));
	dio32.irq4_out_cb().set(FUNC(hp9k3xx_state::dio_irq4_w));
	dio32.irq5_out_cb().set(FUNC(hp9k3xx_state::dio_irq5_w));
	dio32.irq6_out_cb().set(FUNC(hp9k3xx_state::dio_irq6_w));
	dio32.irq7_out_cb().set(FUNC(hp9k3xx_state::dio_irq7_w));

	DIO32_SLOT(config, "sl0", 0, "diobus", dio16_cards, "human_interface", true);
	DIO32_SLOT(config, "sl1", 0, "diobus", dio16_cards, "98544", false);
	DIO32_SLOT(config, "sl2", 0, "diobus", dio16_cards, "98603b", false);
	DIO32_SLOT(config, "sl3", 0, "diobus", dio16_cards, "98644", false);
	DIO32_SLOT(config, "sl4", 0, "diobus", dio32_cards, "98620", false);
	DIO32_SLOT(config, "sl5", 0, "diobus", dio16_cards, nullptr, false);
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k330)
	MCFG_DEVICE_ADD(m_maincpu, M68020PMMU, 16670000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k330_map)

	hp9k300(config);

	bus::hp_dio::dio16_device &dio32(DIO32(config, "diobus", 0));
	dio32.set_cputag(m_maincpu);

	dio32.irq1_out_cb().set(FUNC(hp9k3xx_state::dio_irq1_w));
	dio32.irq2_out_cb().set(FUNC(hp9k3xx_state::dio_irq2_w));
	dio32.irq3_out_cb().set(FUNC(hp9k3xx_state::dio_irq3_w));
	dio32.irq4_out_cb().set(FUNC(hp9k3xx_state::dio_irq4_w));
	dio32.irq5_out_cb().set(FUNC(hp9k3xx_state::dio_irq5_w));
	dio32.irq6_out_cb().set(FUNC(hp9k3xx_state::dio_irq6_w));
	dio32.irq7_out_cb().set(FUNC(hp9k3xx_state::dio_irq7_w));

	DIO32_SLOT(config, "sl0", 0, "diobus", dio16_cards, "human_interface", true);
	DIO32_SLOT(config, "sl1", 0, "diobus", dio16_cards, "98544", false);
	DIO32_SLOT(config, "sl2", 0, "diobus", dio16_cards, "98603b", false);
	DIO32_SLOT(config, "sl3", 0, "diobus", dio16_cards, "98644", false);
	DIO32_SLOT(config, "sl4", 0, "diobus", dio16_cards, nullptr, false);
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k332)
	MCFG_DEVICE_ADD(m_maincpu, M68020PMMU, 16670000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k332_map)

	hp9k300(config);

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_UPDATE_DRIVER(hp9k3xx_state, hp_medres_update)
	MCFG_SCREEN_SIZE(512,390)
	MCFG_SCREEN_VISIBLE_AREA(0, 512-1, 0, 390-1)
	MCFG_SCREEN_REFRESH_RATE(70)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k340)
	hp9k320(config);

	MCFG_DEVICE_REPLACE(m_maincpu, M68030, 16670000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k330_map)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k370)
	hp9k320(config);

	MCFG_DEVICE_REPLACE(m_maincpu, M68030, 33000000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k370_map)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k380)
	hp9k320(config);

	MCFG_DEVICE_REPLACE(m_maincpu, M68040, 25000000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k380_map)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(hp9k3xx_state::hp9k382)
	hp9k320(config);

	MCFG_DEVICE_REPLACE(m_maincpu, M68040, 25000000)
	MCFG_DEVICE_PROGRAM_MAP(hp9k382_map)
MACHINE_CONFIG_END

ROM_START( hp9k310 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "1818-3771.bin", 0x000001, 0x008000, CRC(b9e4e3ad) SHA1(ed6f1fad94a15d95362701dbe124b52877fc3ec4) )
	ROM_LOAD16_BYTE( "1818-3772.bin", 0x000000, 0x008000, CRC(a3665919) SHA1(ec1bc7e5b7990a1b09af947a06401e8ed3cb0516) )

	ROM_REGION( 0x4000, "graphics", ROMREGION_ERASEFF | ROMREGION_BE )
	ROM_LOAD16_BYTE( "98544_1818-1999.bin", 0x000000, 0x002000, CRC(8c7d6480) SHA1(d2bcfd39452c38bc652df39f84c7041cfdf6bd51) )
ROM_END

ROM_START( hp9k320 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "5061-6538.bin", 0x000001, 0x004000, CRC(d6aafeb1) SHA1(88c6b0b2f504303cbbac0c496c26b85458ac5d63) )
	ROM_LOAD16_BYTE( "5061-6539.bin", 0x000000, 0x004000, CRC(a7ff104c) SHA1(c640fe68314654716bd41b04c6a7f4e560036c7e) )
	ROM_LOAD16_BYTE( "5061-6540.bin", 0x008001, 0x004000, CRC(4f6796d6) SHA1(fd254897ac1afb8628f40ea93213f60a082c8d36) )
	ROM_LOAD16_BYTE( "5061-6541.bin", 0x008000, 0x004000, CRC(39d32998) SHA1(6de1bda75187b0878c03c074942b807cf2924f0e) )
ROM_END

ROM_START( hp9k330 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "1818-4416.bin", 0x000000, 0x010000, CRC(cd71e85e) SHA1(3e83a80682f733417fdc3720410e45a2cfdcf869) )
	ROM_LOAD16_BYTE( "1818-4417.bin", 0x000001, 0x010000, CRC(374d49db) SHA1(a12cbf6c151e2f421da4571000b5dffa3ef403b3) )
ROM_END

ROM_START( hp9k332 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "1818-4796.bin", 0x000000, 0x010000, CRC(8a7642da) SHA1(7ba12adcea85916d18b021255391bec806c32e94) )
	ROM_LOAD16_BYTE( "1818-4797.bin", 0x000001, 0x010000, CRC(98129eb1) SHA1(f3451a854060f1be1bee9f17c5c198b4b1cd61ac) )

	ROM_REGION( 0x4000, "graphics", ROMREGION_ERASEFF | ROMREGION_BE | ROMREGION_32BIT )
	ROM_LOAD16_BYTE( "5180-0471.bin", 0x000001, 0x002000, CRC(7256af2e) SHA1(584e8d4dcae8c898c1438125dc9c4709631b32f7) )
ROM_END

ROM_START( hp9k340 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "1818-4416.bin", 0x000000, 0x010000, CRC(cd71e85e) SHA1(3e83a80682f733417fdc3720410e45a2cfdcf869) )
	ROM_LOAD16_BYTE( "1818-4417.bin", 0x000001, 0x010000, CRC(374d49db) SHA1(a12cbf6c151e2f421da4571000b5dffa3ef403b3) )

ROM_END

ROM_START( hp9k370 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_BYTE( "1818-4416.bin", 0x000000, 0x010000, CRC(cd71e85e) SHA1(3e83a80682f733417fdc3720410e45a2cfdcf869) )
	ROM_LOAD16_BYTE( "1818-4417.bin", 0x000001, 0x010000, CRC(374d49db) SHA1(a12cbf6c151e2f421da4571000b5dffa3ef403b3) )
ROM_END

ROM_START( hp9k380 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_WORD_SWAP( "1818-5062_98754_9000-380_27c210.bin", 0x000000, 0x020000, CRC(500a0797) SHA1(4c0a3929e45202a2689e353657e5c4b58ff9a1fd) )
ROM_END

ROM_START( hp9k382 )
	ROM_REGION( 0x20000, MAINCPU_TAG, 0 )
	ROM_LOAD16_WORD_SWAP( "1818-5468_27c1024.bin", 0x000000, 0x020000, CRC(d1d9ef13) SHA1(6bbb17b9adad402fbc516dc2f3143e9c38ceef8e) )

	ROM_REGION( 0x2000, "unknown", ROMREGION_ERASEFF | ROMREGION_BE | ROMREGION_32BIT )
	ROM_LOAD( "1818-5282_8ce61e951207_28c64.bin", 0x000000, 0x002000, CRC(740442f3) SHA1(ab65bd4eec1024afb97fc2dd3bd3f017e90f49ae) )
ROM_END

/*    YEAR  NAME     PARENT   COMPAT  MACHINE  INPUT    CLASS          INIT        COMPANY            FULLNAME      FLAGS */
COMP( 1985, hp9k310, 0,       0,      hp9k310, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/310", MACHINE_NOT_WORKING)
COMP( 1985, hp9k320, 0,       0,      hp9k320, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/320", MACHINE_NOT_WORKING)
COMP( 1987, hp9k330, 0,       0,      hp9k330, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/330", MACHINE_NOT_WORKING)
COMP( 1987, hp9k332, 0,       0,      hp9k332, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/332", MACHINE_NOT_WORKING)
COMP( 1989, hp9k340, hp9k330, 0,      hp9k340, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/340", MACHINE_NOT_WORKING)
COMP( 1988, hp9k370, hp9k330, 0,      hp9k370, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/370", MACHINE_NOT_WORKING)
COMP( 1991, hp9k380, 0,       0,      hp9k380, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/380", MACHINE_NOT_WORKING)
COMP( 1991, hp9k382, 0,       0,      hp9k382, hp9k330, hp9k3xx_state, empty_init, "Hewlett-Packard", "HP9000/382", MACHINE_NOT_WORKING)

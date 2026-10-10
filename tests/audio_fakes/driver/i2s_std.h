#pragma once
#include <cstdint>
#include <functional>
using i2s_chan_handle_t = void *;
using gpio_num_t = int;
constexpr int ESP_OK=0, I2S_NUM_AUTO=0, I2S_ROLE_MASTER=0, I2S_DATA_BIT_WIDTH_16BIT=0, I2S_SLOT_MODE_STEREO=0, I2S_GPIO_UNUSED=-1;
struct i2s_chan_config_t {};
struct i2s_std_clk_config_t { unsigned rate; };
struct SlotConfig {};
struct i2s_std_config_t {
  i2s_std_clk_config_t clk_cfg{};
  SlotConfig slot_cfg;
  struct { int mclk=0,bclk=0,ws=0,dout=0,din=0; } gpio_cfg;
};
#define I2S_CHANNEL_DEFAULT_CONFIG(a,b) i2s_chan_config_t{}
#define I2S_STD_CLK_DEFAULT_CONFIG(r) i2s_std_clk_config_t{r}
#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(a,b) SlotConfig{}
inline std::function<void(const int16_t *, size_t)> fakeAudioWrite;
inline int i2s_new_channel(const i2s_chan_config_t *, i2s_chan_handle_t *tx, void *) { *tx=reinterpret_cast<void *>(1);return 0; }
inline int i2s_channel_init_std_mode(i2s_chan_handle_t, const i2s_std_config_t *) { return 0; }
inline int i2s_del_channel(i2s_chan_handle_t) { return 0; }
inline int i2s_channel_reconfig_std_clock(i2s_chan_handle_t, const i2s_std_clk_config_t *) { return 0; }
inline int i2s_channel_enable(i2s_chan_handle_t) { return 0; }
inline int i2s_channel_disable(i2s_chan_handle_t) { return 0; }
inline int i2s_channel_write(i2s_chan_handle_t, const void *buffer, size_t bytes, size_t *written, unsigned) {
  *written=bytes;
  if (fakeAudioWrite) fakeAudioWrite(static_cast<const int16_t *>(buffer), bytes/2);
  return 0;
}

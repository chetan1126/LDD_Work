//SPDX_Licence-Identifier: GPL-2.0

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/gpio/consumer.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/property.h>
#include <linux/slab.h>

/*
 * driver private data.
 * one object of this structure is created for one LED device.
 */

struct bbb_ext_led{
	struct gpio_desc *led_gpio; 	// GPIO descriptor for external LED
	struct timer_list blink_timer; 	//kernel timer used for blinking
	unsigned int period_ms;		//Blink period in millisecond
	bool led_state;			// current logical LED state
};

/*
 * Timer callback
 *
 * This function runs periodically
 * It toggles the LED and  rearms the timer.
 *
 * Important:
 * kernel timer callback runs in softirq/atomic context.
 *Therfore, we must not use GPIO APIs that can sleep.
 */


static void bbb_ext_led_timer_cb(struct timer_list *t)
{
	struct bbb_ext_led *led;
	
	led = from_timer(led,t,blink_timer);

	led->led_state = !led->led_state;

	gpiod_set_value(led->led_gpio, led->led_state);

	mod_timer(&led->blink_timer, jiffies + msecs_to_jiffies(led->period_ms));

}

/*
 * Device-managed cleanupfuction.
 *
 * This run automatically when the plTFORM DEVICE IS REMOVED
 * OR  when the driver is unloaded.
 */

static void bbb_ext_led_cleanup(void *date);
{
	struct bbb_ext_led *led;
	
	led = data;

	del_timer_sync(&led->blink_timer);
	
	gpiod_set_value(led->led_gpio,0);
}

 /*
 * probe() is called when:
 *
 * 1. Device Tree creates a platform device with:
 * 	compatible = "chetan, bbb-ext-led"
 *
 * 2. This driver contains the compatible string "chetan, bbb-ext-led" in its of_match_table.
 *
 */

static int bbb_ext_led_probe(struct platform_device *pdev)
{
	struct bbb_ext_led *led;
	int ret;
	struct device *dev;
	u32 period;

	dev = &pdev->dev; 

	dev_info(dev, "BBB external LED probe called\n");

	led = devm_kzalloc(dev, sizeof(*led), GFP_KERNEL);
	if(!led)
	return -ENOMEM;

	/*
	 * Request GPIO from device tree
	 * con_id = "led"
	 *
	 * This maps to device tree property;
	 * 	led-gpios = <&gpio1 28 0>;
	 * 
	 * GPIOD_OUT_LOW configure the GPIO as output
	 * with initial logical value 0 (LED OFF).
	 */

	 led->led_gpio = devm_gpiod_get(dev, "led", GPIOD_OUT_LOW);
	 if(IS_ERR(led->led_gpio))
	 return dev_err_probe(dev, PTR_ERR(led->led_gpio), "Failed to get LED GPIO\n");

	/*
	 * Timer callback cannot sleep.
	 * therefore , reject Gpio controller that reuired sleeping to set GPIO value.
	 */

	 led->period_ms = 500; // default blink period is 1000ms
	 
	 ret = device_property_read_u32(dev, "blink-period-ms", &period_ms);
	 if(ret == 0)
	 led->period_ms = period;
	
	 led->led_state = false; // LED is initially OFF

	 timer_setup(&led->blink_timer, bbb_ext_led_timer_cb, 0);

	 platform_set_drvdata(pdev, led);

	 ret = devm_add_action_or_reset(dev, bbb_ext_led_cleanup, led);
	 if(ret) return ret;

	 mod_timer(&led->blink_timer, jiffies + msecs_to_jiffies(led->period_ms));

	 dev_info(dev, "BBB  external LED blinking started with period = %d ms\n", led->period_ms);

	 return 0;




}



static const struct of_device_id bbb_ext_led_of_match[] ={
	{ .compatible = "chetan, bbb-ext-led"},
	{ }
};
MODULE_DEVICE_TABLE(of, bbb_ext_led_of_match);

/*
 *platform Driver object.
*/
static struct platform_driver bbb_ext_led_driver = {
	.probe = bbb_ext_led_probe,
	.driver = {
		.name = "bbb-ext-led",
		.of_match_table = bbb_ext_led_of_match,
	},
};
/*this macro creates module init and exit functions.
 * module_platform_driver() internally calls platform_driver_register() and platform_driver_unregister().
 */

module_platform_driver(bbb_ext_led_driver); 


MODULE_LICENSE("GPL");
MODULE_AUTHOR("CHETAN");
MODULE_DESCRIPTION("Beaglebone Black External LED Blink Driver using GPIO descriptor API");
MODULE_VERSION("1.0");











































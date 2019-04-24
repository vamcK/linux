/*
 * Randomness driver for virtio
 *  Copyright (C) 2007, 2008 Rusty Russell IBM Corporation
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/hw_random.h>
#include <linux/scatterlist.h>
#include <linux/spinlock.h>
#include <linux/virtio.h>
#include <linux/virtio_nic.h>
#include <linux/module.h>


struct virtnic_info {
	char name[25];
	struct completion have_data;
	bool busy;
	bool nic_register_done;
	bool nic_removed;
};

// static void random_recv_done(struct virtqueue *vq)
// {
// 	struct virtrng_info *vi = vq->vdev->priv;

// 	/* We can get spurious callbacks, e.g. shared IRQs + virtio_pci. */
// 	if (!virtqueue_get_buf(vi->vq, &vi->data_avail))
// 		return;

// 	complete(&vi->have_data);
// }

/* The host will fill any buffer we give it with sweet, sweet randomness. */
// static void register_buffer(struct virtrng_info *vi, u8 *buf, size_t size)
// {
// 	struct scatterlist sg;

// 	sg_init_one(&sg, buf, size);

// 	/* There should always be room for one buffer. */
// 	virtqueue_add_inbuf(vi->vq, &sg, 1, buf, GFP_KERNEL);

// 	virtqueue_kick(vi->vq);
// }

// static int virtio_read(struct hwrng *rng, void *buf, size_t size, bool wait)
// {
// 	int ret;
// 	struct virtrng_info *vi = (struct virtrng_info *)rng->priv;

// 	if (vi->hwrng_removed)
// 		return -ENODEV;

// 	if (!vi->busy) {
// 		vi->busy = true;
// 		init_completion(&vi->have_data);
// 		register_buffer(vi, buf, size);
// 	}

// 	if (!wait)
// 		return 0;

// 	ret = wait_for_completion_killable(&vi->have_data);
// 	if (ret < 0)
// 		return ret;

// 	vi->busy = false;

// 	return vi->data_avail;
// }

// static void virtio_cleanup(struct hwrng *rng)
// {
// 	struct virtrng_info *vi = (struct virtrng_info *)rng->priv;

// 	if (vi->busy)
// 		wait_for_completion(&vi->have_data);
// }


static int virtnic_probe(struct virtio_device *vdev)
{
	printk(KERN_INFO "nic probing started");
	struct virtnic_info *vn = NULL;

	vn = kzalloc(sizeof(struct virtnic_info), GFP_KERNEL);
	if (!vn)
		return -ENOMEM;

	sprintf(vn->name, "virtio_nic");
	init_completion(&vn->have_data);
	vdev->priv = vn;
	printk(KERN_INFO "nic probing ended");
	return 0;
}

static void virtnic_remove(struct virtio_device *vdev)
{
	struct virtnic_info *vn = vdev->priv; // private pointer of driver
	complete(&vn->have_data);
	vdev->config->reset(vdev);
	vn->busy = false;
	// vdev->config->del_vqs(vdev); delete the defined virtqueues
	kfree(vn);
}

static void virtnic_scan(struct virtio_device *vdev)
{
	// struct virtnic_info *vn = vdev->priv;
	// int err;
}


/*
Code related to device going to sleep, we will deal with this later, once
basic setup is done
*/

// #ifdef CONFIG_PM_SLEEP
// static int virtrng_freeze(struct virtio_device *vdev)
// {
// 	remove_common(vdev);
// 	return 0;
// }

// static int virtrng_restore(struct virtio_device *vdev)
// {
// 	int err;

// 	err = probe_common(vdev);
// 	if (!err) {
// 		struct virtrng_info *vi = vdev->priv;

// 		/*
// 		 * Set hwrng_removed to ensure that virtio_read()
// 		 * does not block waiting for data before the
// 		 * registration is complete.
// 		 */
// 		vi->hwrng_removed = true;
// 		err = hwrng_register(&vi->hwrng);
// 		if (!err) {
// 			vi->hwrng_register_done = true;
// 			vi->hwrng_removed = false;
// 		}
// 	}

// 	return err;
// }
// #endif

static struct virtio_device_id id_table[] = {
	{ VIRTIO_ID_NIC, VIRTIO_DEV_ANY_ID },
	{ 0 },
};

static struct virtio_driver virtio_nic_driver = {
	.driver.name =	KBUILD_MODNAME,
	.driver.owner =	THIS_MODULE,
	.id_table =	id_table,
	.probe =	virtnic_probe,
	.remove =	virtnic_remove,
	.scan =		virtnic_scan,
// #ifdef CONFIG_PM_SLEEP
// 	.freeze =	virtrng_freeze,
// 	.restore =	virtrng_restore,
// #endif
};

module_virtio_driver(virtio_nic_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio nic driver");
MODULE_LICENSE("GPL");

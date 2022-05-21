#!/usr/bin/python3

import unittest as ut
import os as os

device_filename = "symbolic_device"
module_filename = "simple_serial_device.ko"


class Test(ut.TestCase):
	def __createDeviceFile__(self):
		res = os.system("touch {0}".format(device_filename));
		self.assertEqual(res, 0, "Cannot create symbolic device file")

	def __deleteDeviceFile__(self):
		res = os.system("rm {0}".format(device_filename));
		self.assertEqual(res, 0, "Cannot remove symbolic device file")

	def __load_module__(self):
		res = os.system("insmod {0}".format(module_filename));
		self.assertEqual(res, 0, "Cannot load module")
	
	def __unload_module__(self):
		res = os.system("rmmod {0}".format(module_filename));
		self.assertEqual(res, 0, "Cannot load module")
	
	def setUp(self):
		self.__createDeviceFile__()

	def tearDown(self):
		self.__deleteDeviceFile__()

	def test_load_unload(self):
		pass

	def est_writeToEmpty(self):
		write_str = "RnVjawo"
		with  open(device_filename, "w") as dev_file:
			res = dev_file.write(write_str)
		self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

	def test_readFromEmpty(self):
		with  open(device_filename, "r") as dev_file:
			read_buff = dev_file.read()
		self.assertEqual(read_buff, "", "Read buffer not empty")

	def test_writeRead(self):
		write_str = "dGhpcwo"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# only for regular, non character-device files

			read_buff = dev_file.read(len(write_str))
			self.assertEqual(read_buff, write_str, "Mismatch between reading and writing")


	def test_writeReadWrite(self):
		write_str = "ZXhhbXBsZQo"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# only for regular, non character-device files

			read_buff = dev_file.read(len(write_str))
			self.assertEqual(read_buff, write_str, "Mismatch between reading and writing")

			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

	def test_writeUntillError(self):
		write_str = "YWdhaW4K"
		with  open(device_filename, "r+") as dev_file:
			for _ in range(0, 4096):
				res = dev_file.write(write_str)
				self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

	def test_readUntillError(self):
		write_str = "YW5kIGFnYWluCg"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# only for regular, non character-device files

			read_buff = dev_file.read(len(write_str) + 1)
			self.assertEqual(read_buff, write_str, "Mismatch between reading and writing")



if __name__ == '__main__':
	ut.main()

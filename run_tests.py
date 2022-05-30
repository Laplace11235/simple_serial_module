#!/usr/bin/python3

from time import sleep
import unittest as ut
import os as os

device_filename   = "/dev/simple_serial_device"
module_name       = "simple_serial_device"
module_filename   = "{0}.ko".format(module_name)
module_parameters = "buffer_size=32"

class Test(ut.TestCase):
	def __load_module__(self):
		res = os.system("insmod {0} {1}".format(module_filename, module_parameters));
		self.assertEqual(res, 0, "Cannot load module")
	
	def __unload_module__(self):
		res = os.system("rmmod {0}".format(module_filename));
		self.assertEqual(res, 0, "Cannot unload module")

	def __check_module_loaded__(self):
		res = os.system("lsmod  | grep {0} > /dev/null".format(module_name));
		self.assertEqual(res, 0, "Cannot check  module loaded")

	def setUp(self):
		self.__load_module__()

	def tearDown(self):
		self.__unload_module__()

	####################### Tests ##############################
	def i_test_load_unload(self):
		self.__check_module_loaded__()

	def test_echo_cat(self):
		write_data = "RnVjawo"
		os.system("echo {0} > {1}".format(write_data, device_filename));
		ret = os.system("cat {0}".format(device_filename));
		self.assertEqual(ret, 0)

	def i_test_open_device_file_read(self):
		with  open(device_filename, "r") as dev_file:
			pass

	def i_test_open_device_file_write(self):
		with  open(device_filename, "w") as dev_file:
			pass

	def i_test_open_device_file_append(self):
		with  open(device_filename, "r+") as dev_file:
			pass

	def i_test_writeToEmpty(self):
		write_str = "RnVjawo"
		with  open(device_filename, "w") as dev_file:
			res = dev_file.write(write_str)
		self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

	def i_test_readFromEmpty(self):
		with  open(device_filename, "r") as dev_file:
			read_buff = dev_file.read(1)
		self.assertEqual(read_buff, "", "Read buffer not empty")

	def i_test_writeRead(self):
		write_str = "dGhpcwo"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# because file opened in non O_DIRECT mode

			read_buff = dev_file.read(len(write_str))
			self.assertEqual(read_buff, write_str, "Mismatch between reading and writing")


	def i_test_writeReadWrite(self):
		write_str = "ZXhhbXBsZQo"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# because file opened in non O_DIRECT mode

			read_buff = dev_file.read(len(write_str))
			self.assertEqual(read_buff, write_str, "Mismatch between reading and writing")

			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

	def i_test_writeUntillError(self):
		write_str = "YWdhaW4K"
		with  open(device_filename, "r+") as dev_file:
			try:
				for _ in range(0, 8196):
					res = dev_file.write(write_str)
			except IOError:
				pass	# if test passed, we should be there
			else:
				self.fail("Operation write should not be permitted")

	def i_test_readUntillError(self):
		write_str = "YW5kIGFnYWluCg"
		with  open(device_filename, "r+") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

			dev_file.seek(0)		# because file opened in non O_DIRECT mode

			dev_file.read()
			buf = dev_file.read()
			self.assertEqual(buf, "")

	def i_test_openWriteCloseOpenRead(self):
		write_str = "YW5kIGFnYWluCg"
		with  open(device_filename, "w") as dev_file:
			res = dev_file.write(write_str)
			self.assertEqual(len(write_str), res, "Cannot write {0} sybmol(s)".format(len(write_str)))

		with  open(device_filename, "r") as dev_file:
			buf = dev_file.read()
			self.assertEqual(buf, write_str, "Mismatch between reading and writing")

if __name__ == '__main__':
	ut.main()

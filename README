Ghost File System or simply GhostFS

File system created to allow the user to access random content of a
remote file which is sitting behind a HTTP webserver.

FUSE and CURL are used in this project, so you will need both packages
installed when getting your hands dirty with this project.

How to use it?
1) Mount the file system:
	./ghostfs /path/to/mount/point
2) Create an empty file:
	touch /path/to/mount/point/<file>
3) Set attribute url of the empty file using extended attribute:
	setfattr -n url -v http://<address> /path/to/mount/point/<file>

And that's all. Now you can access content of /path/to/mount/point/<file> as
if it were local.

For example, let's do that with a remote image file:
	touch /path/to/mount/point/andromeda_galaxy.jpg
	setfattr -n url -v \
		http://raphaelsc.github.io/pictures/andromeda_galaxy.jpg \
		/path/to/mount/point/andromeda_galaxy.jpg
Now you access the image using an image viewer of your choice!

NOTE: In the future, we may replace extended attribute with symbolic link,
so step 2 and 3 would be replaced by:
	ln -s /path/to/mount/point/<file> http://<address>
Simpler, no?

NOTE: We also plan to add support to other protocols, such as FTP and SCP.

For further information, feel free to reach me at:
	raphael.scarv@gmail.com

Thanks.

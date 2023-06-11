.PHONY: unix macosx
-:
	@awk 'BEGIN {FS = ":.*?##"} /^[a-zA-Z_-]+:.*?## / {printf "\033[36m%-15s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
help:## 	
	@awk 'BEGIN {FS = ":.*?###"} /^[a-zA-Z_-]+:.*?###/ {printf "\033[36m%-15s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
unix:## 	
unix:###try: sudo make unix args="install"
	echo $(args)
	@pushd $@ && $(MAKE) $(args)
macosx:unix## 	
macosx:###try: sudo make macosx args="install"
	echo $(args)
	@pushd $@ && $(MAKE) $(args)


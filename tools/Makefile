LEVEL = ..

DIRS = dump_log

include $(LEVEL)/Makefile.common

Scripts = dynaa_check_aa.py dynaa_check_cg.py dynaa_display_pts.py dynaa_utils.py dynaa_hook_mem.py dynaa_insert_alias_checker.py

install-local::
	$(Verb) for script in $(Scripts) ; do \
		echo Installing $$script ; \
		$(MKDIR) $(DESTDIR)$(PROJ_bindir) ; \
		if [ "$$script" = "dynaa_utils.py" ]; then \
		  $(DataInstall) $(PROJ_SRC_DIR)/$$script $(DESTDIR)$(PROJ_bindir) ; \
		else \
		  $(ScriptInstall) $(PROJ_SRC_DIR)/$$script $(DESTDIR)$(PROJ_bindir) ; \
		fi; \
	done

uninstall-local::
	$(Verb) $(RM) -f $(addprefix $(DESTDIR)$(PROJ_bindir)/,$(Scripts))

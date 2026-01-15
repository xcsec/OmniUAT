######## AMD SEV-SNP Settings ########
# Note: This is an example Makefile, actual implementation requires adjustment according to AMD SEV-SNP SDK

SEV_SNP_SDK ?= /opt/amd/sev-snp-sdk
SEV_SNP_ARCH ?= x86_64

######## Common Source Files ########
COMMON_DIR := ../Common
COMMON_SOURCES := $(COMMON_DIR)/mpt_tree/mpt_tree.cpp \
                  $(COMMON_DIR)/sequencer/sequencer.cpp \
                  $(COMMON_DIR)/merkle_crdt/merkle_crdt.cpp \
                  $(COMMON_DIR)/tee_cluster/tee_cluster.cpp

######## Guest VM (Secure World) ########
GUEST_DIR := GuestVM
GUEST_SOURCES := $(GUEST_DIR)/sev_guest.cpp \
                 $(GUEST_DIR)/platform_sev.cpp \
                 $(GUEST_DIR)/sev_vm_communication.cpp \
                 $(COMMON_DIR)/mpt_tree/mpt_tree.cpp \
                 $(COMMON_DIR)/sequencer/sequencer.cpp \
                 $(COMMON_DIR)/merkle_crdt/merkle_crdt.cpp \
                 $(COMMON_DIR)/tee_cluster/tee_cluster.cpp

GUEST_INCLUDE := -I$(GUEST_DIR) \
                 -I$(COMMON_DIR)/mpt_tree \
                 -I$(COMMON_DIR)/sequencer \
                 -I$(COMMON_DIR)/merkle_crdt \
                 -I$(COMMON_DIR)/tee_cluster \
                 -I$(COMMON_DIR)/tee_network \
                 -I$(SEV_SNP_SDK)/include

GUEST_CFLAGS := -fPIC -Wall -m64 $(GUEST_INCLUDE)
GUEST_LDFLAGS := -L$(SEV_SNP_SDK)/lib -lsev_snp_guest -lcrypto

######## Host VM (Normal World) ########
HOST_DIR := HostVM
HOST_SOURCES := $(HOST_DIR)/host_vm_app.cpp \
                $(HOST_DIR)/host_vm_mediator.cpp \
                ../App_Common/l2_simulator/l2_simulator.cpp \
                ../App_Common/network_layer/network_layer.cpp

HOST_INCLUDE := -I$(HOST_DIR) \
                -I../App_Common/l2_simulator \
                -I../App_Common/network_layer \
                -I$(SEV_SNP_SDK)/include

HOST_CFLAGS := -fPIC -Wall -m64 $(HOST_INCLUDE)
HOST_LDFLAGS := -L$(SEV_SNP_SDK)/lib -lsev_snp_host -lpthread

######## Targets ########
.PHONY: all clean guest host

all: guest host

guest: $(GUEST_DIR)/sev_guest.bin
	@echo "Built SEV-SNP Guest VM"

host: $(HOST_DIR)/host_vm_app
	@echo "Built SEV-SNP Host VM App"

$(GUEST_DIR)/sev_guest.bin: $(GUEST_SOURCES) $(COMMON_SOURCES)
	@echo "Building SEV-SNP Guest VM..."
	@echo "Note: Actual build commands depend on AMD SEV-SNP SDK"
	@echo "Guest VM needs to be built as a bootable image"
	# $(CC) $(GUEST_CFLAGS) -o $@ $^ $(GUEST_LDFLAGS)
	# Additional steps: create SEV-SNP image, sign, etc.

$(HOST_DIR)/host_vm_app: $(HOST_SOURCES)
	@echo "Building SEV-SNP Host VM App..."
	@echo "Note: Actual build commands depend on AMD SEV-SNP SDK"
	# $(CXX) $(HOST_CFLAGS) -o $@ $^ $(HOST_LDFLAGS)

clean:
	@rm -f $(GUEST_DIR)/*.bin $(GUEST_DIR)/*.o
	@rm -f $(HOST_DIR)/host_vm_app $(HOST_DIR)/*.o
	@echo "Cleaned SEV-SNP build files"


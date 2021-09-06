
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <malloc.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <chrono>
#include <thread>

//#include "zes_api.h"
//#include "ze_api.h"

#define MAX_PATH_LEN 256

 
#include "hwloc.h"
#include <errno.h>

 
int get_hwloc_topo(uint32_t domain, uint32_t bus, uint32_t device, uint32_t function)
{
    hwloc_topology_t topology;
    hwloc_obj_t obj;
 
    /* Allocate and initialize topology object. */
    hwloc_topology_init(&topology);
    hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
    hwloc_topology_load(topology);
 
  obj = nullptr;
  while ((obj = hwloc_get_next_pcidev(topology, obj)) != nullptr) {
    assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
    if(obj->attr->pcidev.domain == domain &&
      obj->attr->pcidev.bus == bus &&
      obj->attr->pcidev.dev == device &&
      obj->attr->pcidev.func == function) {
          hwloc_obj_t obj2 = obj;
          while( (obj2 = obj2->parent) != nullptr) {
              if(obj2->type == HWLOC_OBJ_BRIDGE){
                if (obj2->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                    assert(obj2->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                    printf("%s!!! Found host->PCI bridge for domain %x bus %x-%x\n",
                        obj2->name,
                        obj2->attr->bridge.downstream.pci.domain,
                        obj2->attr->bridge.downstream.pci.secondary_bus,
                        obj2->attr->bridge.downstream.pci.subordinate_bus);
                } else {
                    assert(obj2->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                    assert(obj2->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                    printf("%s!!! Found PCI->PCI bridge [%x:%x] for domain %x bus %x-%x\n",
                        obj2->name,
                        obj2->attr->bridge.upstream.pci.vendor_id,
                        obj2->attr->bridge.upstream.pci.device_id,
                        obj2->attr->bridge.downstream.pci.domain,
                        obj2->attr->bridge.downstream.pci.secondary_bus,
                        obj2->attr->bridge.downstream.pci.subordinate_bus);
                }
                break;

                hwloc_obj_t objTemp = obj2;
                 while ((objTemp = objTemp->prev_cousin) != nullptr) {
                    if(obj2->type == HWLOC_OBJ_BRIDGE){
                        if (obj2->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
                            assert(obj2->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                            printf("%s!!! Found host->PCI bridge for domain %x bus %x-%x\n",
                                obj2->name,
                                obj2->attr->bridge.downstream.pci.domain,
                                obj2->attr->bridge.downstream.pci.secondary_bus,
                                obj2->attr->bridge.downstream.pci.subordinate_bus);
                        } else {
                            assert(obj2->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
                            assert(obj2->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
                            printf("%s!!! Found PCI->PCI bridge [%x:%x] for domain %x bus %x-%x\n",
                                obj2->name,
                                obj2->attr->bridge.upstream.pci.vendor_id,
                                obj2->attr->bridge.upstream.pci.device_id,
                                obj2->attr->bridge.downstream.pci.domain,
                                obj2->attr->bridge.downstream.pci.secondary_bus,
                                obj2->attr->bridge.downstream.pci.subordinate_bus);
                        }               
                    }
                 }
              }
          }
          
      }
  }


    /* Destroy topology object. */
    hwloc_topology_destroy(topology);
 
    return 0;
}
/*
int test_level_zero() {

	ze_result_t ze_result;
    putenv(const_cast<char *>("ZES_ENABLE_SYSMAN=1"));

    if (zeInit(0) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't initialize the API" << std::endl;
        exit(1);
    }
    uint32_t driver_count = 0;

    if (zeDriverGet(&driver_count, nullptr) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't get driver count" << std::endl;
        exit(1);
    }

    std::vector<ze_driver_handle_t> drivers(driver_count);

    if (zeDriverGet(&driver_count, drivers.data()) != ZE_RESULT_SUCCESS)
    {
        std::cout << "Can't get driver" << std::endl;
        exit(1);
    };

    for (auto &p_driver : drivers)
    {
        uint32_t device_count = 0;
        zeDeviceGet(p_driver, &device_count, nullptr);
        std::vector<ze_device_handle_t> devices(device_count);
        zeDeviceGet(p_driver, &device_count, devices.data());
        ze_driver_properties_t driver_prop;
        zeDriverGetProperties(p_driver, &driver_prop);

        for (auto &device : devices)
        {
            zes_device_handle_t zes_Device = (zes_device_handle_t)device;
            zes_device_properties_t props = {};
            props.stype = ZES_STRUCTURE_TYPE_DEVICE_PROPERTIES;
            zesDeviceGetProperties(zes_Device, &props);
            if (props.core.type == ZE_DEVICE_TYPE_GPU)
            {

                zes_pci_properties_t pci_propperties;
                ze_result = zesDevicePciGetProperties(zes_Device, &pci_propperties);
                if (ze_result != ZE_RESULT_SUCCESS)
                {
                    return 1;
                }

				get_hwloc_topo(pci_propperties.address.domain, pci_propperties.address.bus, 
					pci_propperties.address.device, pci_propperties.address.function);
				
            }
        }
    }
    return 0;
}*/


static void print_self(hwloc_obj_t obj)
{
    if(obj->type == HWLOC_OBJ_BRIDGE) {
        /* only host->pci and pci->pci bridge supported so far */
        if (obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_HOST) {
        assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
        printf(" Found (count:%d:) host->PCI bridge for domain %x bus %x-%x\n",
            obj->io_arity, 
          //  obj->attr->bridge.upstream.pci.vendor_id,
          //  obj->attr->bridge.upstream.pci.device_id,
            obj->attr->bridge.downstream.pci.domain,
            obj->attr->bridge.downstream.pci.secondary_bus,
            obj->attr->bridge.downstream.pci.subordinate_bus);
        } else {
        assert(obj->attr->bridge.upstream_type == HWLOC_OBJ_BRIDGE_PCI);
        assert(obj->attr->bridge.downstream_type == HWLOC_OBJ_BRIDGE_PCI);
        printf("- Found (count:%d:) PCI->PCI bridge [%x:%x] for domain %x bus %x-%x\n",
            obj->io_arity,
            obj->attr->bridge.upstream.pci.vendor_id,
            obj->attr->bridge.upstream.pci.device_id,
            obj->attr->bridge.downstream.pci.domain,
            obj->attr->bridge.downstream.pci.secondary_bus,
            obj->attr->bridge.downstream.pci.subordinate_bus);
        }
    } else {
        printf("unknow type >>>>> %d\n", obj->type);
    }
}

static void print_obj(hwloc_obj_t obj)
{
    if(obj->type == HWLOC_OBJ_PCI_DEVICE){
        printf(" Found PCI (%04x %02x:%02x.%x) PCI device class %x  [%x:%x]\n",
            obj->attr->pcidev.domain, obj->attr->pcidev.bus,obj->attr->pcidev.dev,obj->attr->pcidev.func,
	        obj->attr->pcidev.class_id, obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);
    } else {
        printf("unknow type <<<<<<< %d\n", obj->type);
    }
}

static void walk_children(hwloc_topology_t topology, hwloc_obj_t obj)
{
    char type[32];
    hwloc_obj_type_snprintf(type, sizeof(type), obj, 0);
    printf("%*s%s", 2, "", type);
    printf("->");
    print_self(obj);

    printf("\n");

    hwloc_obj_t objChild = obj->io_first_child;

    if(objChild == NULL) {
        return;
    }
    printf("begin ::::::::::::::::::::::::::::::::::::::\n");
    while(objChild != NULL) {
        print_obj(objChild);
        objChild = objChild->next_sibling;
    }
    printf("end ::::::::::::::::::::::::::::::::::::::\n");
}
 
void test_topo() {
    hwloc_topology_t topology;
 

  hwloc_topology_init(&topology);
  hwloc_topology_set_io_types_filter(topology, HWLOC_TYPE_FILTER_KEEP_ALL);
  hwloc_topology_load(topology);

 // printf("*** **********************************************\n");
 //   walk_children(topology, hwloc_get_root_obj(topology));
//printf("*** **********************************************e\n");


 hwloc_obj_t obj;
  printf("Found %d bridges\n", hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_BRIDGE));
  obj = nullptr;
  while ((obj = hwloc_get_next_bridge(topology, obj)) != NULL) {
    
    walk_children(topology, obj);
  }
/*
  printf("Found %d PCI devices\n", hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_PCI_DEVICE));
  obj = nullptr;
  while ((obj = hwloc_get_next_pcidev(topology, obj)) != nullptr) {
    assert(obj->type == HWLOC_OBJ_PCI_DEVICE);
    printf(" Found (%s) PCI device class %x vendor %x model %x\n",
        obj->name,
	   obj->attr->pcidev.class_id, obj->attr->pcidev.vendor_id, obj->attr->pcidev.device_id);
  }

  printf("Found %d OS devices\n", hwloc_get_nbobjs_by_type(topology, HWLOC_OBJ_OS_DEVICE));
  obj = nullptr;
  while ((obj = hwloc_get_next_osdev(topology, obj)) != nullptr) {
    assert(obj->type == HWLOC_OBJ_OS_DEVICE);
    printf(" Found OS device %s subtype %d\n", obj->name, obj->attr->osdev.type);
  }
*/
  assert(HWLOC_TYPE_DEPTH_BRIDGE == hwloc_get_type_depth(topology, HWLOC_OBJ_BRIDGE));
  assert(HWLOC_TYPE_DEPTH_PCI_DEVICE == hwloc_get_type_depth(topology, HWLOC_OBJ_PCI_DEVICE));
  assert(HWLOC_TYPE_DEPTH_OS_DEVICE == hwloc_get_type_depth(topology, HWLOC_OBJ_OS_DEVICE));
  assert(hwloc_compare_types(HWLOC_OBJ_BRIDGE, HWLOC_OBJ_PCI_DEVICE) < 0);
  assert(hwloc_compare_types(HWLOC_OBJ_BRIDGE, HWLOC_OBJ_OS_DEVICE) < 0);
  assert(hwloc_compare_types(HWLOC_OBJ_PCI_DEVICE, HWLOC_OBJ_OS_DEVICE) < 0);

  hwloc_topology_destroy(topology);
}


int main() {
	//test_level_zero();
    test_topo();
}

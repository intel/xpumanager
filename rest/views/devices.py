import stub
from flask import jsonify
from marshmallow import Schema, fields


class DeviceBasicInfoSchema(Schema):
    device_id = fields.Int() 
    device_type = fields.Str()
    uuid = fields.Str()
    device_name = fields.Str()
    pci_device_id = fields.Str()
    pci_sub_device_id = fields.Str()
    pci_bdf_address = fields.Str()
    vendor_name = fields.Str()



def get_devices():
    """
    Get device list.
    ---
    get:
        description: Get device list
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: 
                    type: array
                    items: DeviceBasicInfoSchema
            500:
                description: Error
    """
    code, message, data = stub.getDeviceList()
    if code == 0:
        # return jsonify(data)
        schema = DeviceBasicInfoSchema()
        data = schema.jsonify(data)
        return data
    else:
        return message, 500


class DevicePropertiesSchema(Schema):
    device_id = fields.Int()
    device_type = fields.Str()
    device_name = fields.Str()
    vendor_name = fields.Str()
    uuid = fields.Str()
    pci_vendor_id = fields.Str()
    pci_bdf_address = fields.Str()
    pci_slot = fields.Str()
    pcie_generation = fields.Str()
    pcie_max_link_width = fields.Str()
    driver_version = fields.Str()
    firmware_name = fields.Str()
    firmware_version = fields.Str()
    serial_number = fields.Str()
    core_clock_rate_mhz = fields.Str()
    memory_physical_size_byte = fields.Str()
    memory_free_size_byte = fields.Str()
    max_mem_alloc_size_byte = fields.Str()
    number_of_memory_channels = fields.Str()
    memory_bus_width = fields.Str()
    max_hardware_contexts = fields.Str()
    max_command_queue_priority = fields.Str()
    number_of_tiles = fields.Str()
    number_of_slices = fields.Str()
    number_of_sub_slices_per_slice = fields.Str()
    number_of_eus_per_sub_slice = fields.Str()
    number_of_threads_per_eu = fields.Str()
    physical_eu_simd_width = fields.Str()
    
    

def get_device_properties(deviceId):
    """
    Get device detail properties.
    ---
    get:
        description: Get device properties
        parameters:
            - 
                name: deviceId
                in: path
                description: Device id
                type: integer
        produces: 
            - application/json
        responses:
            200:
                description: OK
                schema: DevicePropertiesSchema
            400:
                description: Error
            500:
                description: Error
    """
    try:
        code, message, data = stub.getDeviceProperties(int(deviceId))
        if code == 0:
            return jsonify(data)
        error = dict(Status=code, Message=message)
        return jsonify(error), 400
    except Exception as e:
        print(e)
        return "Internal error", 500

#include "volk.h"

struct VulkanGraphicsPipeline {
	VkPipeline pipeline;
};

//Might not even be necessary?
// struct VulkanVertexInputState {

// };

struct VulkanInputAssemblyState {
    VkPipelineInputAssemblyStateCreateFlags    flags = 0;
    VkPrimitiveTopology                        topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkBool32                                   primitiveRestartEnable = VK_FALSE;
};

struct VulkanTesselationState {
    VkPipelineTessellationStateCreateFlags    flags                      = 0;
    uint32_t                                  patchControlPoints         = 0;
};

//Viewport and scissor should always be dynamic, there's no reason not to
struct VulkanViewportState {
    VkPipelineViewportStateCreateFlags    flags             = 0;
    uint32_t                              viewportCount     = 1;
    const VkViewport*                     pViewports        = nullptr;
    uint32_t                              scissorCount      = 1;
    const VkRect2D*                       pScissors         = nullptr;    
};

struct VulkanRasterizationState {
    VkPipelineRasterizationStateCreateFlags    flags                            = 0;
    VkBool32                                   depthClampEnable                 = VK_FALSE;
    VkBool32                                   rasterizerDiscardEnable          = VK_FALSE;
    VkPolygonMode                              polygonMode                      = VK_POLYGON_MODE_FILL;   
    VkCullModeFlags                            cullMode                         = VK_CULL_MODE_BACK_BIT;
    VkFrontFace                                frontFace                        = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkBool32                                   depthBiasEnable                  = VK_FALSE;
    float                                      depthBiasConstantFactor          = 0.0;
    float                                      depthBiasClamp                   = 0.0;
    float                                      depthBiasSlopeFactor             = 0.0;
    float                                      lineWidth                        = 1.0;
};

struct VulkanMultisampleState {
    VkPipelineMultisampleStateCreateFlags    flags                          = 0;
    VkSampleCountFlagBits                    rasterizationSamples           = VK_SAMPLE_COUNT_1_BIT;
    VkBool32                                 sampleShadingEnable            = VK_FALSE;
    float                                    minSampleShading               = 0.0;
    const VkSampleMask*                      pSampleMask                    = nullptr;
    VkBool32                                 alphaToCoverageEnable          = VK_FALSE;
    VkBool32                                 alphaToOneEnable               = VK_FALSE;
};

struct VulkanDepthStencilState {
    VkPipelineDepthStencilStateCreateFlags    flags                     = 0;
    VkBool32                                  depthTestEnable           = VK_TRUE;
    VkBool32                                  depthWriteEnable          = VK_TRUE;
    VkCompareOp                               depthCompareOp            = VK_COMPARE_OP_LESS_OR_EQUAL;
    VkBool32                                  depthBoundsTestEnable     = VK_FALSE;
    VkBool32                                  stencilTestEnable         = VK_FALSE;
    VkStencilOpState                          front                     = {};
    VkStencilOpState                          back                      = {};
    float                                     minDepthBounds            = 0.0;
    float                                     maxDepthBounds            = 0.0;
};

struct VulkanColorBlendAttachmentState {
    VkBool32                 blendEnable                = VK_TRUE;
    VkBlendFactor            srcColorBlendFactor        = VK_BLEND_FACTOR_SRC_COLOR;
    VkBlendFactor            dstColorBlendFactor        = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
    VkBlendOp                colorBlendOp               = VK_BLEND_OP_ADD;
    VkBlendFactor            srcAlphaBlendFactor        = VK_BLEND_FACTOR_SRC_ALPHA;     
    VkBlendFactor            dstAlphaBlendFactor        = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    VkBlendOp                alphaBlendOp               = VK_BLEND_OP_ADD;
    VkColorComponentFlags    colorWriteMask             = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
};

struct VulkanColorBlendState {
    VkPipelineColorBlendStateCreateFlags          flags                 = 0;
    VkBool32                                      logicOpEnable         = VK_FALSE;
    VkLogicOp                                     logicOp               = VK_LOGIC_OP_CLEAR;
    uint32_t                                      attachmentCount       = 0;
    const VulkanColorBlendAttachmentState*        pAttachments          = nullptr;
    float                                         blendConstants[4]     = {1.0, 1.0, 1.0, 1.0};
};

